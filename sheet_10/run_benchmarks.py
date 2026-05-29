#!/usr/bin/env python3

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

MEMORY_LIMIT = 8 * 1024 * 1024 * 1024  # 8 GiB
OUTPUT_FILE = Path("benchmark_results.csv")

# DATA_STRUCTURES = ["array", "linked_list_sequential", "linked_list_random", "unrolled_linked_list"]
# INS_DEL_FRACTIONS = ["0", "1", "10", "50"]
# ELEMENT_SIZES = [("8B", 8), ("512B", 512), ("8MB", 8 * 1024 * 1024)]
# ELEMENT_COUNTS = [10, 1000, 100000, 10000000]
# ACCESS_PATTERN = ["SEQ", "RAND"]

DATA_STRUCTURES = ["unrolled_linked_list"]
INS_DEL_FRACTIONS = ["1"]
ELEMENT_SIZES = [("8B", 8)]
ELEMENT_COUNTS = [1000]
ACCESS_PATTERN = ["SEQ", "RAND"]

SCRIPT_DIR = Path(__file__).resolve().parent

def benchmark_executable() -> Path:
    for exe in SCRIPT_DIR.glob("containerBenchmark*"):
        if exe.is_file() and os.access(exe, os.X_OK):
            return exe

    raise FileNotFoundError(
        f"Benchmark executable not found in {SCRIPT_DIR}. "
        "Place the executable in the same directory as this script and name it containerBenchmark."
    )

def estimated_bytes(ds: str, elem_bytes: int, count: int) -> int:
    overhead = 64 if ds == "array" else 96
    return count * (elem_bytes + overhead)

def should_skip(ds: str, elem_bytes: int, count: int) -> bool:
    return estimated_bytes(ds, elem_bytes, count) > MEMORY_LIMIT

def main() -> int:
    exe = benchmark_executable()
    OUTPUT_FILE.write_text(
        "label,elements,bytes,ins_del_percent,ops_per_second,access_pattern,checksum\n"
    )

    for ds in DATA_STRUCTURES:
        for ins_del in INS_DEL_FRACTIONS:
            for size_label, elem_bytes in ELEMENT_SIZES:
                for count in ELEMENT_COUNTS:
                    for pattern in ACCESS_PATTERN:
                        if should_skip(ds, elem_bytes, count):
                            skipped = (
                                f"# skipped {ds} {ins_del} {size_label} {count} {pattern} "
                                f"estimated={estimated_bytes(ds, elem_bytes, count)}\n"
                            )
                            with OUTPUT_FILE.open("a", encoding="utf-8") as handle:
                                handle.write(skipped)
                            continue

                        result = subprocess.run(
                            [str(exe), ds, ins_del, size_label, str(count), pattern],
                            check=True,
                            capture_output=True,
                            text=True,
                        )
                        with OUTPUT_FILE.open("a", encoding="utf-8") as handle:
                            handle.write(result.stdout)
                            if result.stderr:
                                handle.write(result.stderr)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())