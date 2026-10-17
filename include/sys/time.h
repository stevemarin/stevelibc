#ifndef SYS_TIME_H
#define SYS_TIME_H

#include "../sys/types.h"

/*
 * Linux x86_64 timeval ABI: seconds since the Unix epoch plus a microsecond
 * fraction in [0, 1,000,000). This is wall-clock time, not an elapsed timer.
 */
struct timeval {
  time_t tv_sec;
  suseconds_t tv_usec;
};

/*
 * Read wall-clock time since the Unix epoch. tv may be null when the caller
 * only needs the legacy operation's success result. The timezone argument is
 * obsolete and ignored by Linux; pass null in new code.
 */
int gettimeofday(struct timeval *tv, void *timezone);

#endif
