#!/bin/bash 

#SBATCH --job-name=JOB.NAME
#SBATCH --qos=debug
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=1
#SBATCH --cpus-per-task=1
#SBATCH --time=00-00:00:01
#SBATCH --exclusive

export OMP_NUM_THREADS=$SLURM_CPUS_PER_TASK

# If MPI
srun COMMAND [parameters]
# If not
COMMAND [parameters]
