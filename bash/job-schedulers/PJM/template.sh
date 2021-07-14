#!/bin/bash 

#PJM -N JOB.NAME
#PJM -L elapse=00:20:00
#PJM -L node=1
# Number of total MPI ranks and ranks per node
#PJM --mpi "proc=10,max-proc-per-node=10"

export OMP_NUM_THREADS=10

# If MPI
mpirun COMMAND [parameters]
# If not
COMMAND [parameters]
