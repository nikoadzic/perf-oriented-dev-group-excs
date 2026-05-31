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

# Load modules if necessary
# module load gcc python

# Ensure the executable exists before running
if [ ! -f ./containerBenchmark ]; then
    echo "Error: containerBenchmark not found. Compile it first with:"
    echo "g++ -O3 -std=c++17 -Wall -Wextra containerBenchmark.cpp -o containerBenchmark"
    exit 1
fi

# Run the python tuner
# The script automatically detects SLURM_ARRAY_TASK_ID and SLURM_ARRAY_TASK_COUNT
python3 tune_tiered.py
