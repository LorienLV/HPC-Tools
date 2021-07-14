#define _GNU_SOURCE 1

#include "PerfStopwatch.h"

#include "printer.h"

#include <asm/unistd.h>
#include <errno.h>
#include <linux/perf_event.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <iostream>
#include <utility>

// File descriptor used by perf.
int PerfStopwatch::fd[NUM_EVENTS] = {-1};

// tracked_events[i] > 0 if the is being tracked, 0 otherwise.
unsigned int PerfStopwatch::tracked_events[NUM_EVENTS] = {0};

// Event description.
const char *PerfStopwatch::descriptors[NUM_EVENTS] = {
    "cpu cycles",
    "instructions",
    "cache references",
    "cache misses",
    "branch instructions",
    "branch misses",
    "bus cycles",
    "stalled cycles frontend",
    "stalled cycles backend",
    "ref cpu cycles",

    "L1D read access",
    "L1I read access",
    "LL read access",
    "DTLB read access",
    "ITLB read access",
    "BPU read access",
    "NODE read access",

    "L1D read misses",
    "L1I read misses",
    "LL read misses",
    "DTLB read misses",
    "ITLB read misses",
    "BPU read misses",
    "NODE read misses",

    "L1D write access",
    "L1I write access",
    "LL write access",
    "DTLB write access",
    "ITLB write access",
    "BPU write access",
    "NODE write access",

    "L1D write misses",
    "L1I write misses",
    "LL write misses",
    "DTLB write misses",
    "ITLB write misses",
    "BPU write misses",
    "NODE write misses",

    "L1D prefetch access",
    "L1I prefetch access",
    "LL prefetch access",
    "DTLB prefetch access",
    "ITLB prefetch access",
    "BPU prefetch access",
    "NODE prefetch access",

    "L1D prefetch misses",
    "L1I prefetch misses",
    "LL prefetch misses",
    "DTLB prefetch misses",
    "ITLB prefetch misses",
    "BPU prefetch misses",
    "NODE prefetch misses",

    "cpu clock",
    "task clock",
    "page faults",
    "context switches",
    "cpu migrations",
    "page faults min",
    "page faults maj",
    "alignment faults",
    "emulation faults",
    "dummy",
    "bpf output",
};

PerfStopwatch::PerfStopwatch(const std::vector<Event> &req_events_) :
    req_events(req_events_) {
    start_count.resize(req_events.size());
    total_count.resize(req_events.size());

    perf_start(req_events);

    restart();
}

PerfStopwatch::PerfStopwatch(const PerfStopwatch &other) :
    req_events(other.req_events),
    start_count(other.start_count),
    total_count(other.total_count) {

    perf_start(req_events);
}

PerfStopwatch::PerfStopwatch(PerfStopwatch &&other) noexcept :
    req_events(std::move(other.req_events)),
    start_count(std::move(other.start_count)),
    total_count(std::move(other.total_count)) {}

PerfStopwatch &PerfStopwatch::operator=(const PerfStopwatch &other) {
    return *this = PerfStopwatch(other);
}

PerfStopwatch &PerfStopwatch::operator=(PerfStopwatch &&other) noexcept {
    std::swap(req_events, other.req_events);
    std::swap(start_count, other.start_count);
    std::swap(total_count, other.total_count);

    return *this;
}

PerfStopwatch::~PerfStopwatch() {
    // "Turn off" no longer needed hw counters.
    for (const auto &event : req_events) {

        tracked_events[event] -= 1;

        if (fd[event] != -1 && tracked_events[event] == 0) {
            ioctl(fd[event], PERF_EVENT_IOC_DISABLE, 0);
            close(fd[event]);

            fd[event] = -1;
        }
    }
}

void PerfStopwatch::restart() {
    for (auto &count : total_count) {
        count = 0;
    }
}

void PerfStopwatch::play() {
    // "Turn off" hw counters.
    for (const auto &event : tracked_events) {
        if (fd[event] != -1) {
            ioctl(fd[event], PERF_EVENT_IOC_DISABLE, 0);
        }
    }

    // Reading HW counter.
    for (size_t i = 0; i < req_events.size(); ++i) {
        const auto &event = req_events[i];

        if (fd[event] == -1) {
            continue;
        }

        if (sizeof(uint64_t) != read(fd[event], &start_count[i], sizeof(uint64_t))) {
            print_error("%16s: ERROR reading perf event.\n", descriptors[event]);
        }
    }

    // Reactivate counters.
    for (const auto &event : tracked_events) {
        if (fd[event] != -1) {
            ioctl(fd[event], PERF_EVENT_IOC_ENABLE, 0);
        }
    }
}

void PerfStopwatch::pause() {
    // "Turn off" hw counters.
    for (const auto &event : tracked_events) {
        if (fd[event] != -1) {
            ioctl(fd[event], PERF_EVENT_IOC_DISABLE, 0);
        }
    }

    // Reading HW counter.
    for (size_t i = 0; i < req_events.size(); ++i) {
        const auto &event = req_events[i];

        if (fd[event] == -1) {
            continue;
        }

        uint64_t stop_count;

        if (sizeof(uint64_t) != read(fd[event], &stop_count, sizeof(uint64_t))) {
            print_error("%16s: ERROR reading perf event.\n", descriptors[event]);
        }
        else {
            total_count[i] += (stop_count - start_count[i]);
        }
    }

    // Reactivate counters.
    for (const auto &event : tracked_events) {
        if (fd[event] != -1) {
            ioctl(fd[event], PERF_EVENT_IOC_ENABLE, 0);
        }
    }
}

void PerfStopwatch::print_all_counters() const {
    for (const auto &event : req_events) {
        if (fd[event] != -1) {
            printf("%16s: %14lu\n", descriptors[event], total_count[event]);
        }
    }
}

uint64_t PerfStopwatch::get_counter(const Event target_event) const {
    for (size_t i = 0; i < req_events.size(); ++i) {
        const auto &event = req_events[i];

        if (event == target_event && fd[event] != -1) {
            return total_count[i];
        }
    }

    throw std::runtime_error("PerfStopwatch: Trying to read a non tracked event");
}

const char *PerfStopwatch::get_descriptor(const Event target_event) {
    return descriptors[target_event];
}

int PerfStopwatch::perf_event_open(const struct perf_event_attr *const hw_event, const pid_t pid,
                                   const int cpu, const int group_fd, const unsigned long flags) {
    return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

void PerfStopwatch::perf_struct(struct perf_event_attr *const pe, const int type, const int config) {

    memset(pe, 0, sizeof(struct perf_event_attr));
    pe->size = sizeof(struct perf_event_attr);
    pe->disabled = 1;

    // Exclude kernel and hypervisor from being measured.
    pe->exclude_kernel = 1;
    pe->exclude_hv = 1;

    // Children inherit it
    pe->inherit = 1;

    // Type of event to measure.
    pe->type = type;
    pe->config = config;
}

/**
 * Initialize and activate HW event counters.
 *
 * @param num_events Number of events to track.
 * @param req_events Events to track.
 *
 */
void PerfStopwatch::perf_start(const std::vector<Event> &req_events) {
    static struct perf_event_attr pe[NUM_EVENTS];

    // Type of events. This must match the event structure!!
    static const int types[NUM_EVENTS] = {
        PERF_TYPE_HARDWARE,
        PERF_TYPE_HARDWARE,
        PERF_TYPE_HARDWARE,
        PERF_TYPE_HARDWARE,
        PERF_TYPE_HARDWARE,
        PERF_TYPE_HARDWARE,
        PERF_TYPE_HARDWARE,
        PERF_TYPE_HARDWARE,
        PERF_TYPE_HARDWARE,
        PERF_TYPE_HARDWARE,

        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,
        PERF_TYPE_HW_CACHE,

        PERF_TYPE_SOFTWARE,
        PERF_TYPE_SOFTWARE,
        PERF_TYPE_SOFTWARE,
        PERF_TYPE_SOFTWARE,
        PERF_TYPE_SOFTWARE,
        PERF_TYPE_SOFTWARE,
        PERF_TYPE_SOFTWARE,
        PERF_TYPE_SOFTWARE,
        PERF_TYPE_SOFTWARE,
        PERF_TYPE_SOFTWARE,
        PERF_TYPE_SOFTWARE,
    };

    // Name of the events to measure
    static const int events[NUM_EVENTS] = {
        /* Hardware events */
        PERF_COUNT_HW_CPU_CYCLES,
        PERF_COUNT_HW_INSTRUCTIONS,
        PERF_COUNT_HW_CACHE_REFERENCES,
        PERF_COUNT_HW_CACHE_MISSES,
        PERF_COUNT_HW_BRANCH_INSTRUCTIONS,
        PERF_COUNT_HW_BRANCH_MISSES,
        PERF_COUNT_HW_BUS_CYCLES,
        PERF_COUNT_HW_STALLED_CYCLES_FRONTEND,
        PERF_COUNT_HW_STALLED_CYCLES_BACKEND,
        PERF_COUNT_HW_REF_CPU_CYCLES,

        /* Cache events */

        // L1D Read access
        PERF_COUNT_HW_CACHE_L1D |
        (PERF_COUNT_HW_CACHE_OP_READ << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),

        // L1I Read access
        PERF_COUNT_HW_CACHE_L1I |
        (PERF_COUNT_HW_CACHE_OP_READ << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),

        // LL Read access
        PERF_COUNT_HW_CACHE_LL |
        (PERF_COUNT_HW_CACHE_OP_READ << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),

        // DTLB Read access
        PERF_COUNT_HW_CACHE_DTLB |
        (PERF_COUNT_HW_CACHE_OP_READ << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),

        // ITLB Read access
        PERF_COUNT_HW_CACHE_ITLB |
        (PERF_COUNT_HW_CACHE_OP_READ << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),

        // BPU Read access
        PERF_COUNT_HW_CACHE_BPU |
        (PERF_COUNT_HW_CACHE_OP_READ << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),

        // NODE Read access
        PERF_COUNT_HW_CACHE_NODE |
        (PERF_COUNT_HW_CACHE_OP_READ << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),

        //

        // L1D Read misses
        PERF_COUNT_HW_CACHE_L1D |
        (PERF_COUNT_HW_CACHE_OP_READ << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),

        // L1I Read misses
        PERF_COUNT_HW_CACHE_L1I |
        (PERF_COUNT_HW_CACHE_OP_READ << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),

        // LL Read misses
        PERF_COUNT_HW_CACHE_LL |
        (PERF_COUNT_HW_CACHE_OP_READ << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),

        // DTLB Read misses
        PERF_COUNT_HW_CACHE_DTLB |
        (PERF_COUNT_HW_CACHE_OP_READ << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),

        // ITLB Read misses
        PERF_COUNT_HW_CACHE_ITLB |
        (PERF_COUNT_HW_CACHE_OP_READ << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),

        // BPU Read misses
        PERF_COUNT_HW_CACHE_BPU |
        (PERF_COUNT_HW_CACHE_OP_READ << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),

        // NODE Read misses
        PERF_COUNT_HW_CACHE_NODE |
        (PERF_COUNT_HW_CACHE_OP_READ << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),

        //

        // L1D Write access
        PERF_COUNT_HW_CACHE_L1D |
        (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),

        // L1I Write access
        PERF_COUNT_HW_CACHE_L1I |
        (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),

        // LL Write access
        PERF_COUNT_HW_CACHE_LL |
        (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),

        // DTLB Write access
        PERF_COUNT_HW_CACHE_DTLB |
        (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),

        // ITLB Write access
        PERF_COUNT_HW_CACHE_ITLB |
        (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),

        // BPU Write access
        PERF_COUNT_HW_CACHE_BPU |
        (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),

        // NODE Write access
        PERF_COUNT_HW_CACHE_NODE |
        (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),

        //

        // L1D Write misses
        PERF_COUNT_HW_CACHE_L1D |
        (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),

        // L1I Write misses
        PERF_COUNT_HW_CACHE_L1I |
        (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),

        // LL Write misses
        PERF_COUNT_HW_CACHE_LL |
        (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),

        // DTLB Write misses
        PERF_COUNT_HW_CACHE_DTLB |
        (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),

        // ITLB Write misses
        PERF_COUNT_HW_CACHE_ITLB |
        (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),

        // BPU Write misses
        PERF_COUNT_HW_CACHE_BPU |
        (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),

        // NODE Write misses
        PERF_COUNT_HW_CACHE_NODE |
        (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),

        //

        // L1D Prefetch access
        PERF_COUNT_HW_CACHE_L1D |
        (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),

        // L1I Prefetch access
        PERF_COUNT_HW_CACHE_L1I |
        (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),

        // LL Prefetch access
        PERF_COUNT_HW_CACHE_LL |
        (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),

        // DTLB Prefetch access
        PERF_COUNT_HW_CACHE_DTLB |
        (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),

        // ITLB Prefetch access
        PERF_COUNT_HW_CACHE_ITLB |
        (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),

        // BPU Prefetch access
        PERF_COUNT_HW_CACHE_BPU |
        (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),

        // NODE Prefetch access
        PERF_COUNT_HW_CACHE_NODE |
        (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),

        //

        // L1D Prefetch misses
        PERF_COUNT_HW_CACHE_L1D |
        (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),

        // L1I Prefetch misses
        PERF_COUNT_HW_CACHE_L1I |
        (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),

        // LL Prefetch misses
        PERF_COUNT_HW_CACHE_LL |
        (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),

        // DTLB Prefetch misses
        PERF_COUNT_HW_CACHE_DTLB |
        (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),

        // ITLB Prefetch misses
        PERF_COUNT_HW_CACHE_ITLB |
        (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),

        // BPU Prefetch misses
        PERF_COUNT_HW_CACHE_BPU |
        (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),

        // NODE Prefetch misses
        PERF_COUNT_HW_CACHE_NODE |
        (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),

        /* Software events */
        PERF_COUNT_SW_CPU_CLOCK,
        PERF_COUNT_SW_TASK_CLOCK,
        PERF_COUNT_SW_PAGE_FAULTS,
        PERF_COUNT_SW_CONTEXT_SWITCHES,
        PERF_COUNT_SW_CPU_MIGRATIONS,
        PERF_COUNT_SW_PAGE_FAULTS_MIN,
        PERF_COUNT_SW_PAGE_FAULTS_MAJ,
        PERF_COUNT_SW_ALIGNMENT_FAULTS,
        PERF_COUNT_SW_EMULATION_FAULTS,
        PERF_COUNT_SW_DUMMY,
        PERF_COUNT_SW_BPF_OUTPUT,
    };

    // Open perf leader.
    for (const auto &event : req_events) {
        if (tracked_events[event] == 0) {

            perf_struct(&pe[event], types[event], events[event]);

            fd[event] = perf_event_open(&pe[event], 0, -1, -1, 0);

            if (fd[event] == -1) {
                print_error("Error opening event %llx (%s) %s\n",
                            pe[event].config, descriptors[event], strerror(errno));
            }
            else {
                // Enable descriptor to read hw counters.
                ioctl(fd[event], PERF_EVENT_IOC_RESET, 0);
            }
        }

        tracked_events[event] += 1;
    }
}