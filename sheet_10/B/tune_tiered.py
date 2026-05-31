import os
import subprocess
import sys
import itertools
from pathlib import Path

MEMORY_LIMIT = 8 * 1024 * 1024 * 1024  # 8 GiB

# Get SLURM environment variables
job_idx = int(os.environ.get("SLURM_ARRAY_TASK_ID", 0))
num_jobs = int(os.environ.get("SLURM_ARRAY_TASK_COUNT", 1))

OUTPUT_FILE = Path(f"tuning_results_tiered_{job_idx}.csv")

# Parameter Space
DATA_STRUCTURES = ["tiered_array"]
ALL_FRACTIONS = ["0", "1", "10", "50"]
ELEMENT_SIZES = [("8B", 8), ("512B", 512), ("8MB", 8 * 1024 * 1024)]
ELEMENT_COUNTS = [10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000]
CHUNK_SIZES = [8, 16, 32, 64, 128, 256, 512, 1024, 2048]

def benchmark_executable():
    exe = Path("containerBenchmark.exe") if os.name == "nt" else Path("containerBenchmark")
    if not exe.exists():
        raise FileNotFoundError("Benchmark executable not found: {}. Must be in the same directory.".format(exe))
    return exe.resolve()

def estimated_bytes(ds, elem_bytes, count):
    overhead = 64 if ds == "array" else 96
    return count * (elem_bytes + overhead)

def should_skip(ds, elem_bytes, count):
    return estimated_bytes(ds, elem_bytes, count) > MEMORY_LIMIT

def main():
    exe = benchmark_executable()
    
    # Generate ALL possible combinations
    all_configs = list(itertools.product(DATA_STRUCTURES, ALL_FRACTIONS, ELEMENT_SIZES, ELEMENT_COUNTS, CHUNK_SIZES))
    
    # Slice the configs for THIS job using a stride
    my_configs = all_configs[job_idx::num_jobs]
    
    print(f"Job {job_idx}/{num_jobs} handling {len(my_configs)} configs")

    # Initialize the file with the header
    with OUTPUT_FILE.open("w", encoding="utf-8") as handle:
        handle.write("label,elements,bytes,ins_del_percent,ops_per_second,checksum,chunk_size\n")

    for ds, ins_del, (size_label, elem_bytes), count, chunk_size in my_configs:
        if should_skip(ds, elem_bytes, count):
            skipped = (
                "# skipped {} {} {} {} estimated={}\n".format(
                    ds, ins_del, size_label, count, estimated_bytes(ds, elem_bytes, count)
                )
            )
            with OUTPUT_FILE.open("a", encoding="utf-8") as handle:
                handle.write(skipped)
            continue

        print("Running: {} | {}% mix | {} | N={} | Chunk: {}".format(
            ds, ins_del, size_label, count, chunk_size))
        
        try:
            result = subprocess.run(
                [str(exe), ds, ins_del, str(elem_bytes), str(count), "SEQ", str(chunk_size)],
                check=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=True,
            )
            
            with OUTPUT_FILE.open("a", encoding="utf-8") as handle:
                if result.stdout:
                    for line in result.stdout.strip().split('\n'):
                        parts = line.split(',')
                        if len(parts) >= 6:
                            # Reconstruct the line without the access_pattern (which is index 5)
                            # Result stdout format: label,elements,bytes,ins_del_percent,ops_per_second,access_pattern,checksum
                            # CSV format: label,elements,bytes,ins_del_percent,ops_per_second,checksum,chunk_size
                            clean_line = f"{parts[0]},{parts[1]},{parts[2]},{parts[3]},{parts[4]},{parts[6]}"
                            handle.write("{},{}\n".format(clean_line, chunk_size))
                if result.stderr:
                    handle.write(result.stderr)
        except subprocess.CalledProcessError as e:
            print(f"Error running config: {e}")
            with OUTPUT_FILE.open("a", encoding="utf-8") as handle:
                handle.write(f"# ERROR: {ds} {ins_del} {size_label} {count} {chunk_size}\n")

    print("\nTuning complete! Results saved to {}".format(OUTPUT_FILE.name))
    return 0

if __name__ == "__main__":
    sys.exit(main())
