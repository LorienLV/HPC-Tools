#!/bin/bash

# A regression test template for SLURM and PJM job-schedulers

# Clean the stage folder of the jobs after finishing? 1 -> yes, 0 -> no.
clean=1

# The name of the job.
job="test"

# job additional parameters.
# job_options=(
#     #    '--exclusive'
#     # '--time=00:00:01'
#     # '--qos=debug'
# )

# Commands to run.
# You can access the number of mpi-ranks using the environment variable
# MPI_RANKS and the number of omp-threads using the environment variable
# OMP_NUM_THREADS:
# commands=(
#    'command $MPI_RANKS $OMP_NUM_THREADS'
#)
# OR
# commands=(
#    "command \$MPI_RANKS \$OMP_NUM_THREADS"
#)
commands=(
    'time -p sleep 1'
    'time -p sleep 2'
    'time -p sleep 3'
)

# Additional arguments to pass to the commands.
command_opts=""

# Nodes, MPI ranks and OMP theads used to execute with each command.
parallelism=(
    'nodes=1, mpi=1, omp=1'
    # 'nodes=4, mpi=5, omp=6'
)

#
# This function is executed before launching a job. You can use this function to
# prepare the stage folder of the job.
#
before_run() (
    job_name="$1"
)

#
# This function is executed when a job has finished. You can use this function to
# perform a sanity check and to output something in the report of a job.
#
# echo: The message that you want to output in the report of the job.
# return: 0 if the run is correct, 1 otherwise.
#
after_run() (
    job_name="$1"

    wall_time="$(tac "$job_name.err" | grep -m 1 "real" | cut -d ' ' -f 2)"

    echo "Wall time: $wall_time s"

    return 0 # OK
)

source regression.sh
