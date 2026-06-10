#!/bin/bash

# Execute job in the partition "lva" unless you have special requirements.
#SBATCH --partition=lva
# Name your job to be able to identify it later
#SBATCH --job-name fibBench
# Redirect output stream to this file
#SBATCH --output=outputs/fibBench_%a.log
# Maximum number of tasks (=processes) to start in total
#SBATCH --ntasks=1
# Maximum number of tasks (=processes) to start per node
#SBATCH --ntasks-per-node=1
# Enforce exclusive node allocation, do not share with other jobs
#SBATCH --exclusive
#SBATCH --array=0-4

# Define the list of scripts
SCRIPTS=(
    "./lua-5.5.0/src/lua fib.lua"
    "./lua-5.5.0/src/lua fib.lua"
    "./lua-5.5.0/src/lua fib.lua"
    "./lua-5.5.0/src/lua fib.lua"
    "./lua-5.5.0/src/lua fib.lua"
)

# Get the script corresponding to this task's ID
SCRIPT_TO_RUN=${SCRIPTS[$SLURM_ARRAY_TASK_ID]}

echo "Starting job $SLURM_ARRAY_TASK_ID: running $SCRIPT_TO_RUN"

$SCRIPT_TO_RUN