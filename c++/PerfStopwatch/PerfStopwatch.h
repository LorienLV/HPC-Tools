#pragma once

#include <stdint.h>
#include <unistd.h>
#include <vector>

/**
 * @author Lorién López Villellas (lorien.lopez@bsc.es)
 *
 * A module for counting hardware events using perf.
 * A perf_stopwatch works the same way a regular stopwatch does,
 * you can restart the stopwatch, start counting events by playing it,
 * or stop counting events by pausing it.
 *
 * Warning: In order to tracks events for every thread, the main thread must
 * be the one controlling the stopwatch.
 */

class PerfStopwatch {
public:
    enum Event {
        CPU_CYCLES,
        INSTRUCTIONS,
        CACHE_REFERENCES,
        CACHE_MISSES,
        BRANCH_INSTRUCTIONS,
        BRANCH_MISSES,
        BUS_CYCLES,
        STALLED_CYCLES_FRONTEND,
        STALLED_CYCLES_BACKEND,
        REF_CPU_CYCLES,

        L1D_READ_ACCESS,
        L1I_READ_ACCESS,
        LL_READ_ACCESS,
        DTLB_READ_ACCESS,
        ITLB_READ_ACCESS,
        BPU_READ_ACCESS,
        NODE_READ_ACCESS,

        L1D_READ_MISSES,
        L1I_READ_MISSES,
        LL_READ_MISSES,
        DTLB_READ_MISSES,
        ITLB_READ_MISSES,
        BPU_READ_MISSES,
        NODE_READ_MISSES,

        L1D_WRITE_ACCESS,
        L1I_WRITE_ACCESS,
        LL_WRITE_ACCESS,
        DTLB_WRITE_ACCESS,
        ITLB_WRITE_ACCESS,
        BPU_WRITE_ACCESS,
        NODE_WRITE_ACCESS,

        L1D_WRITE_MISSES,
        L1I_WRITE_MISSES,
        LL_WRITE_MISSES,
        DTLB_WRITE_MISSES,
        ITLB_WRITE_MISSES,
        BPU_WRITE_MISSES,
        NODE_WRITE_MISSES,

        L1D_PREFETCH_ACCESS,
        L1I_PREFETCH_ACCESS,
        LL_PREFETCH_ACCESS,
        DTLB_PREFETCH_ACCESS,
        ITLB_PREFETCH_ACCESS,
        BPU_PREFETCH_ACCESS,
        NODE_PREFETCH_ACCESS,

        L1D_PREFETCH_MISSES,
        L1I_PREFETCH_MISSES,
        LL_PREFETCH_MISSES,
        DTLB_PREFETCH_MISSES,
        ITLB_PREFETCH_MISSES,
        BPU_PREFETCH_MISSES,
        NODE_PREFETCH_MISSES,

        CPU_CLOCK,
        TASK_CLOCK,
        PAGE_FAULTS,
        CONTEXT_SWITCHES,
        CPU_MIGRATIONS,
        PAGE_FAULTS_MIN,
        PAGE_FAULTS_MAJ,
        ALIGNMENT_FAULTS,
        EMULATION_FAULTS,
        DUMMY,
        BPF_OUTPUT,

        NUM_EVENTS
    };

    PerfStopwatch() = delete; // No default constructor allowed.

    /**
     * Initializes the stopwatch. Also performs a restart().
     *
     * @param req_events perf events to track.
     */
    PerfStopwatch(const std::vector<Event> &req_events_);

    /**
     * Copy constructor.
     *
     * @param other Other PerfStopwatch.
     */
    PerfStopwatch(const PerfStopwatch &other);

    /**
     * Move constructor.
     *
     * @param other Other PerfStopwatch.
     */
    PerfStopwatch(PerfStopwatch &&other) noexcept;

    /**
     * Copy operator.
     *
     * @param other Other PerfStopwatch.
     * @return PerfStopwatch& A reference to a PerfStopwatch.
     */
    PerfStopwatch &operator=(const PerfStopwatch &other);

    /**
     * Move operator.
     *
     * @param other Other PerfStopwatch.
     * @return PerfStopwatch& A reference to a PerfStopwatch.
     */
    PerfStopwatch &operator=(PerfStopwatch &&other) noexcept;

    /**
     * Destroys the stopwatch.
     *
     */
    ~PerfStopwatch();

    /**
     * Restarts the stopwatch counters.
     *
     */
    void restart();

    /**
     * Start counting HW events.
     *
     */
    void play();

    /**
     * Stop counting HW events.
     *
     */
    void pause();

    /**
     * Print the counter of every tracked event into stdout.
     *
     */
    void print_all_counters() const;

    /**
     * Get the PSW event counter referred by EVENT.
     *
     * If the stopwatch is not tracking the target event, the function
     * will throw an exception.
     *
     * @param target_event event reference.
     */
    uint64_t get_counter(const Event target_event) const;

    /**
     * Get the event descriptor referred by EVENT.
     *
     * @param target_event event reference.
     * @return const char* (event descriptor).
     */
    static const char *get_descriptor(const Event target_event);
private:
    // File descriptor used by perf.
    static int fd[NUM_EVENTS];

    // tracked_events[i] > 0 if the is being tracked, 0 otherwise.
    static unsigned int tracked_events[NUM_EVENTS];

    // Event description.
    static const char *descriptors[NUM_EVENTS];

    std::vector<Event> req_events; // Events being tracked.

    std::vector<uint64_t> start_count; // HW counters on play time.
    std::vector<uint64_t> total_count; // Total count between plays and stops.

    /**
     * Creates a file descriptor that allows measuring performance information.
     * Each file descriptor corresponds to one event that is measured; these can
     * be grouped together to measure multiple events simultaneously.
     *
     * @param hw_event structure that provides detailed configuration information
     *                 for the event being created.
     * @param pid PID to monitor.
     * @param cpu CPU to monitor.
     * @param group_fd The group_fd argument allows event groups to be created.
     *                 An event group has one event which is the group leader.
     *                 The leader is created first, with group_fd = -1. The rest of
     *                 the group members are created
     *                 with subsequent perf_event_open() calls with group_fd being
     *                 set to the file descriptor of the group leader.  (A single
     *                 event on its own is created with group_fd = -1 and is
     *                 considered to be a group with only 1 member.)  An event group
     *                 is scheduled onto the CPU as a unit: it will be put onto the
     *                 CPU only if all of the events in the group can be put onto
     *                 the CPU.  This means that the values of the member events can
     *                 be meaningfully compared—added, divided (to get ratios), and
     *                 so on—with each other, since they have counted events for the
     *                 same set of executed instructions.
     * @param flags The flags argument is formed by ORing together zero or more of
     *              the following values:
     *
     *              PERF_FLAG_FD_CLOEXEC (since Linux 3.14)
     *                       This flag enables the close-on-exec flag for the
     *                       created event file descriptor, so that the file
     *                       descriptor is automatically closed on execve(2).
     *                       Setting the close-on-exec flags at creation time,
     *                       rather than later with fcntl(2), avoids potential
     *                       race conditions where the calling thread invokes
     *                       perf_event_open() and fcntl(2) at the same time as
     *                       another thread calls fork(2) then execve(2).
     *
     *               PERF_FLAG_FD_NO_GROUP
     *                       This flag tells the event to ignore the group_fd
     *                       parameter except for the purpose of setting up output
     *                       redirection using the PERF_FLAG_FD_OUTPUT flag.
     *
     *               PERF_FLAG_FD_OUTPUT (broken since Linux 2.6.35)
     *                       This flag re-routes the event's sampled output to
     *                       instead be included in the mmap buffer of the event
     *                       specified by group_fd.
     *
     *               PERF_FLAG_PID_CGROUP (since Linux 2.6.39)
     *                       This flag activates per-container system-wide
     *                       monitoring. A container is an abstraction that
     *                       isolates a set of resources for finer-grained control
     *                       (CPUs, memory, etc.). In this mode, the event is
     *                       measured only if the thread running on the monitored
     *                       CPU belongs to the designated container (cgroup). The
     *                       cgroup is identified by passing a file descriptor
     *                       opened on its directory in the cgroupfs filesystem.
     *                       For instance, if the cgroup to monitor is called test,
     *                       then a file descriptor opened on /dev/cgroup/test
     *                       (assuming cgroupfs is mounted on /dev/cgroup) must be
     *                       passed as the pid parameter. cgroup monitoring is
     *                       available only for system-wide events and may therefore
     *                       require
     *                       extra permissions.
     * @return file descriptor, for use in subsequent system calls (read(2),
     *         mmap(2), prctl(2), fcntl(2), etc.).
     */
    int perf_event_open(const struct perf_event_attr *const hw_event, const pid_t pid,
                        const int cpu, const int group_fd, const unsigned long flags);

    /**
     * Creates a perf struct for tracking the HW events.
     *
     * @param pe perf structure.
     * @param type overall event type (see perf_event_attr man).
     * @param config This specifies which event you want, in conjunction with the
     *               type field. The config1 and config2 fields are also taken
     *               into account in cases where 64 bits is not enough to fully
     *               specify the event. The encoding of these fields are event
     *               dependent (see perf_event_attr man).
     */
    void perf_struct(struct perf_event_attr *const pe, const int type, const int config);

    /**
     * Initialize and activate HW event counters.
     *
     * @param num_events Number of events to track.
     * @param req_events Events to track.
     *
     */
    void perf_start(const std::vector<Event> &req_events);
};