#pragma once

#include <sys/time.h>

/**
 * @author Lorién López Villellas (lorien.lopez@bsc.es)
 *
 * A module for counting time the same way a stopwatch does. You can restart,
 * play or stop a stopwatch.
 */

class TimeStopwatch {
public:
    /**
     * Construct a TimeStopwatch. Also calls restart().
     *
     */
    TimeStopwatch();

    /**
     * Restart the stopwatch (set total_secs to 0).
     *
     */
    void restart();

    /**
     * Start counting time.
     *
     */
    void play();

    /**
     * Stop counting time and add the current counted time to the total counted time.
     *
     */
    void pause();

    /**
     * Print to stdout the total counted time in seconds.
     *
     */
    void print_s() const;

    /**
     * Get the total counted time in seconds.
     *
     */
    double get_s() const;

private:
    timeval start; // Last play timestamp.
    double total_secs;    // Total seconds counted.
};