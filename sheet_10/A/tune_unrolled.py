#!/usr/bin/env python3

import os
import subprocess
import sys
from pathlib import Path

MEMORY_LIMIT = 8 * 1024 * 1024 * 1024  # 8 GiB
OUTPUT_FILE = Path("tuning_results-2.csv")

# Only test the unrolled list
DATA_STRUCTURES = ["unrolled_linked_list"]
INS_DEL_FRACTIONS = ["0", "1", "10", "50"]
ELEMENT_SIZES = [("8B", 8), ("512B", 512), ("8MB", 8 * 1024 * 1024)]
ELEMENT_COUNTS = [10, 1000, 100000, 10000000]

# The different maxChunkSizes we want to evaluate
CHUNK_SIZES = [256, 512, 1024]
# 16, 32, 64, 128 

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
    
    # Initialize the file with the header
    with OUTPUT_FILE.open("w", encoding="utf-8") as handle:
        handle.write("label,elements,bytes,ins_del_percent,ops_per_second,checksum,chunk_size\n")

    for ds in DATA_STRUCTURES:
        for ins_del in INS_DEL_FRACTIONS:
            for size_label, elem_bytes in ELEMENT_SIZES:
                for count in ELEMENT_COUNTS:
                    if should_skip(ds, elem_bytes, count):
                        skipped = (
                            "# skipped {} {} {} {} estimated={}\n".format(
                                ds, ins_del, size_label, count, estimated_bytes(ds, elem_bytes, count)
                            )
                        )
                        with OUTPUT_FILE.open("a", encoding="utf-8") as handle:
                            handle.write(skipped)
                        continue

                    # Loop through chunk sizes
                    for chunk_size in CHUNK_SIZES:
                        print("Running: {} | {}% mix | {} | N={} | Chunk: {}".format(
                            ds, ins_del, size_label, count, chunk_size))
                        
                        result = subprocess.run(
                            [str(exe), ds, ins_del, str(elem_bytes), str(count), str(chunk_size)],
                            check=True,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            universal_newlines=True,
                        )
                        
                        with OUTPUT_FILE.open("a", encoding="utf-8") as handle:
                            if result.stdout:
                                for line in result.stdout.strip().split('\n'):
                                    handle.write("{},{}\n".format(line, chunk_size))
                            if result.stderr:
                                handle.write(result.stderr)

    print("\nTuning complete! Results saved to {}".format(OUTPUT_FILE.name))
    return 0

if __name__ == "__main__":
    sys.exit(main())