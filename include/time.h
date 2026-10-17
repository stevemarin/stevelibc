#ifndef TIME_H
#define TIME_H

#include "./sys/types.h"

/* Linux clock identifiers accepted by clock_gettime in this API subset. */
typedef int clockid_t;
#define CLOCK_REALTIME 0  /* Wall-clock time since the Unix epoch; may be adjusted */
#define CLOCK_MONOTONIC 1 /* Elapsed-time clock that never moves backward */

/*
 * Linux x86_64 timespec ABI: whole seconds plus a nanosecond fraction in
 * [0, 1,000,000,000). The kernel writes this exact field layout.
 */
struct timespec {
  time_t tv_sec;
  long tv_nsec;
};

/*
 * Read a kernel clock into caller-provided storage. CLOCK_REALTIME is calendar
 * time and can jump; CLOCK_MONOTONIC is normally the right choice for durations.
 */
int clock_gettime(clockid_t clock_id, struct timespec *time);

/*
 * Suspend the calling thread for at least the requested interval, subject to
 * scheduler delay. On EINTR, return -1 and report unslept time through remain
 * when it is non-null; the caller may resume sleeping with that remainder.
 */
int nanosleep(const struct timespec *request, struct timespec *remain);

#endif
