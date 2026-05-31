#!/bin/bash

#SBATCH --job-name=tiered_tuning
#SBATCH --output=tuning_%A_%a.out
#SBATCH --error=tuning_%A_%a.err
#SBATCH --partition=lva
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --exclusive
#SBATCH --time=00:30:00
#SBATCH --array=0-7

if [ ! -f ./containerBenchmark ]; then
    echo "Error: containerBenchmark not found. Compile it first with:"
    echo "g++ -O3 -std=c++17 -Wall -Wextra containerBenchmark.cpp -o containerBenchmark"
    exit 1
fi

python3 tune_tiered.py
