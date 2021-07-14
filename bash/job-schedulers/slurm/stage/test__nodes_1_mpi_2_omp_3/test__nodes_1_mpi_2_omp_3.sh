#!/bin/bash
#SBATCH --job-name=test__nodes_1_mpi_2_omp_3
#SBATCH --nodes=1
#SBATCH --ntasks=2
#SBATCH --cpus-per-task=3
#SBATCH --output=test__nodes_1_mpi_2_omp_3.out
#SBATCH --error=test__nodes_1_mpi_2_omp_3.err
#SBATCH --exclusive
#SBATCH --time=00:01:00
#SBATCH --qos=debug
export OMP_NUM_THREADS=3
time -p uno 
