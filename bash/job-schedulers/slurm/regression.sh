#!/bin/bash

# A regression test template for the slurm job-scheduler

# Use srun/mpirun? 1 -> yes, 0 -> no.
mpienable=0
# Clean the output files after finishing? 1 -> yes, 0 -> no.
clean=1

# The name of the job.
job="test"

# Slurm job additional parameters.
job_options=(
#    '--exclusive'
    '--time=00:00:01'
    '--qos=debug'
)

# Commands to run.
commands=(
    'sleep 10'
    'sleep 12'
    'sleep 120'
)

# Additional arguments to pass to the commands.
executable_opts=""

# Noes, MPI ranks and OMP theads used to execute with each command.
parallelism=(
    'nodes=1, mpi=1, omp=1' 
    # 'nodes=4, mpi=5, omp=6'
)

#
# This is executed when a job has finished. You can use this function to
# perform a sanity check and to output something in the report of a job.
# 
# echo: The message that you want to output in the report of the job.
# return: 1 if the run is correct, 0 otherwise.
#
after_run() {
    # Can access $scriptfolder
    job_name="$1"

    echo "GREAT!"
    
    return 0 # Ok
}

################################################################################
#                DO NOT TOUCH ANYTHING BELOW THIS COMMENT                      #
################################################################################

#
# Starting the jobs.
#

export scriptfolder="$(realpath $0)"
workfolder="$(pwd)"

njobs=$(( ${#commands[@]} * ${#parallelism[@]} ))

echo "[Setup]"
echo "    working directory: '$workfolder'"
echo "    script path:       '$scriptfolder'"
echo ""
echo "[==========] Running $njobs job(s)"
echo "[==========] Started on $(date)"
echo ""

nfailed_jobs=0
jobs_status=()
jobs_id=()
jobs_name=()
jobs_nodes=()
jobs_mpi=()
jobs_omp=()
for bin in "${commands[@]}"; do
    for par in "${parallelism[@]}"; do
        nodes="$(echo "$par" | sed -n -E 's/.*nodes[ ]*=[ ]*([0-9]+).*/\1/p')"
        mpi="$(echo "$par" | sed -n -E 's/.*mpi[ ]*=[ ]*([0-9]+).*/\1/p')"
        omp="$(echo "$par" | sed -n -E 's/.*omp[ ]*=[ ]*([0-9]+).*/\1/p')"

        jobs_nodes+=("$nodes")
        jobs_mpi+=("$mpi")
        jobs_omp+=("$omp")

        if [[ -z "$nodes" || "$nodes" -lt 1 ]]; then
            nodes=1
            echo "\033[33mWarning: The number of nodes is $nodes, which is invalid, using 1 instead\e[0m"
        fi
        if [[ -z "$mpi" || "$mpi" -lt 1 ]]; then
            mpi=1
            echo "\033[33mWarning: The number of MPI ranks is $mpi, which is invalid, using 1 instead\e[0m"
        fi
        if [[ -z "$omp" || "$omp" -lt 1 ]]; then
            omp=1
            echo "\033[33mWarning: The number of OpenMP threads is $omp, which is invalid, using 1 instead\e[0m"
        fi

        bin_trilled="$(echo "$bin" | tr -dc '[:alnum:]\n\r')"
        date_compact="$(date '+%Y%m%d_%H%M%S')"
        jobname="${job}_${bin_trilled}_nodes_${nodes}_mpi_${mpi}_omp_${omp}_${date_compact}"

        mkdir "$jobname" > /dev/null 2>&1

        # Create the job script
        jobscript="#!/bin/bash\n"
        jobscript+="#SBATCH --job-name=$jobname\n"
        jobscript+="#SBATCH --nodes=$nodes\n"
        jobscript+="#SBATCH --ntasks=$mpi\n"
        jobscript+="#SBATCH --cpus-per-task=$omp\n"
        jobscript+="#SBATCH --output=${jobname}.out\n"
        jobscript+="#SBATCH --error=${jobname}.err\n"

        for job_option in "${job_options[@]}"; do
            jobscript+="#SBATCH $job_option\n"
        done
    
        jobscript+="export OMP_NUM_THREADS=$omp\n"

        jobscript+="time -p "

        if [ "$mpienable" -eq 1 ]; then
            jobscript+="srun "
        fi

        jobscript+="$bin $executable_opts"

        initial_folder="$(pwd)"
        cd "$jobname"
        echo -e "$jobscript" > "${jobname}.sh"
        sbatch_out="$(sbatch "${jobname}.sh" 2>&1)"

        if [ "$?" -ne 0 ]; then
            echo "[----------]"
            echo -e "[ \033[31m FAILED \e[0m ] Started processing $jobname"
            echo "[----------]"
            echo ""
            echo "$sbatch_out"

            nfailed_jobs=$((nfailed_jobs + 1))
            jobs_status+="F" # Failed
        else
            jobs_name+=("$jobname")

            job_id="$(echo "$sbatch_out" | cut -d ' ' -f4)"
            jobs_id+=("$job_id")
    
            echo "[----------]"
            echo -e "[ \033[32m RUN \e[0m    ] Started processing $jobname"
            echo "[----------]"
            echo ""

            jobs_status+="R" # Ready
        fi

        cd "$initial_folder"
    done
done

echo "==================== Waiting for spawned jobs to finish ======================"
echo ""

#
# Wait for jobs to finish.
#
for i in "${!jobs_id[@]}"; do
    status="${jobs_status[i]}"

    if [[ "$status" == "F" ]]; then
        # Already failed, do not run sanity check.
        continue
    fi

    job_id="${jobs_id[i]}"
    job_name="${jobs_name[i]}"

    nodes="${jobs_nodes[i]}"
    mpi="${jobs_mpi[i]}"
    omp="${jobs_omp[i]}"

    # Wait for job to finish
    slurmstate=""
    while : ; do
        sleep 1
        slurmstate="$(sacct -p -n -j $job_id | grep "^$job_id|" | cut -d '|' -f 6)"
        if [[ -n "$slurmstate" && "$slurmstate" != "PENDING" && "$slurmstate" != "RUNNING" && \
               "$slurmstate" != "REQUEUED" && "$slurmstate" != "RESIZING" && \
               "$slurmstate" != "SUSPENDED" && "$slurmstate" != "REVOKED" ]]; then
            break
        fi
    done

    status="$slurmstate"
    wall_time="NULL"
    after_run_out=""

    if [[ "$slurmstate" == "COMPLETED" ]]; then
        #
        # Sanity check
        #
        after_run_out="$(after_run "$job_name")"

        if [[ $? -eq 0 ]]; then
            status="OK"

            wall_time="$(tac "$job_name/$job_name.err" | grep -m 1 "real" | cut -d ' ' -f 2)"
        else
            status="SANITY CHECK FAILED"
            nfailed_jobs=$((nfailed_jobs + 1))
        fi
    else
        nfailed_jobs=$((nfailed_jobs + 1))
    fi

    status_string=""
    if [[ "$status" == "OK" ]]; then
        status_string="\033[32m${status}\e[0m"
    else
        status_string="\033[31m${status}\e[0m"
    fi

    echo "------------------------------------------------------------------------------"
    echo "$job_name"
    echo "    - Nodes: $nodes"
    echo "    - MPI ranks: $mpi"
    echo "    - OMP threads: $omp"
    echo -e "    - Status: $status_string"
    echo "    - Total wall time: $wall_time"
    echo "Message:"
    echo "$after_run_out"
done

echo ""
echo "[==========]"

if [[ $nfailed_jobs -eq 0 ]]; then 
    # All jobs passed
    echo -e "[\033[32m  PASSED  \e[0m] $nfailed_jobs/$njobs jobs have failed"
else
    # Some job has failed
    echo -e "[\033[31m  FAILED  \e[0m] $nfailed_jobs/$njobs jobs have failed"
fi

echo "[==========] Finished on $(date)"

# Clean all the stage directories.
if [[ $clean -eq 1 ]]; then
    for job_name in "${jobs_name[@]}"; do
        rm -rf "$job_name"
    done
fi