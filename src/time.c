#include "../include/syscall.h"
#include "../include/time.h"
#include "../include/sys/time.h"

/*
 * These wrappers deliberately pass the public x86_64 timespec layout directly
 * to Linux. The kernel owns clock selection, timekeeping, and interruption.
 */
int clock_gettime(clockid_t clock_id, struct timespec *time) {
  return (int)__syscall_result(syscall2(SYS_clock_gettime, clock_id, (long)time));
}

int nanosleep(const struct timespec *request, struct timespec *remain) {
  return (int)__syscall_result(syscall2(SYS_nanosleep, (long)request, (long)remain));
}

/*
 * The older gettimeofday interface returns epoch time at microsecond
 * resolution. Linux accepts a null timezone pointer; the legacy timezone
 * structure is no longer maintained, so pass the caller's optional pointer
 * through without interpreting it.
 */
int gettimeofday(struct timeval *time, void *timezone) {
  return (int)__syscall_result(syscall2(SYS_gettimeofday, (long)time, (long)timezone));
}
