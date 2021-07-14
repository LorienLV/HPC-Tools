#include "TimeStopwatch.h"

#include <stdio.h>
#include <stdlib.h>

TimeStopwatch::TimeStopwatch() : total_secs(0) {}

void TimeStopwatch::restart() {
    total_secs = 0;
}

void TimeStopwatch::play() {
    gettimeofday(&start, NULL);
}

void TimeStopwatch::pause() {
    struct timeval stop;
    gettimeofday(&stop, NULL);
    total_secs += (stop.tv_sec - start.tv_sec)
                  + (stop.tv_usec - start.tv_usec) / 1000000.0;
}

void TimeStopwatch::print_s() const {
    printf("%lf", total_secs);
}

double TimeStopwatch::get_s() const {
    return total_secs;
}