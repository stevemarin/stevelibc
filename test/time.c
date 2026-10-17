#include "../include/errno.h"
#include "../include/stddef.h"
#include "../include/sys/time.h"
#include "../include/time.h"

static int valid_timespec(const struct timespec *value) {
  return value->tv_sec >= 0 && value->tv_nsec >= 0 && value->tv_nsec < 1000000000L;
}

int main(void) {
  /* The kernel writes these public layouts directly: keep their ABI explicit. */
  if (sizeof(struct timeval) != 16 || offsetof(struct timeval, tv_usec) != 8)
    return 11;

  struct timespec before;
  struct timespec after;

  /* Monotonic readings are suitable for elapsed-time comparisons. */
  if (clock_gettime(CLOCK_MONOTONIC, &before) != 0 || !valid_timespec(&before))
    return 1;
  struct timespec request = {.tv_sec = 0, .tv_nsec = 2000000L};
  struct timespec remaining = {.tv_sec = -1, .tv_nsec = -1};
  if (nanosleep(&request, &remaining) != 0)
    return 2;
  if (clock_gettime(CLOCK_MONOTONIC, &after) != 0 || !valid_timespec(&after))
    return 3;
  if (after.tv_sec < before.tv_sec ||
      (after.tv_sec == before.tv_sec && after.tv_nsec < before.tv_nsec))
    return 4;

  /* Realtime has a different epoch but shares the same normalized timespec ABI. */
  struct timespec realtime;
  if (clock_gettime(CLOCK_REALTIME, &realtime) != 0 || !valid_timespec(&realtime) ||
      realtime.tv_sec < 1000000000L)
    return 5;

  /* Invalid clock identifiers and nanosecond ranges are reported by Linux. */
  errno = 0;
  if (clock_gettime(-1, &after) != -1 || errno != EINVAL)
    return 6;
  errno = 0;
  if (nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = 1000000000L}, 0) != -1 ||
      errno != EINVAL)
    return 7;
  errno = 0;
  if (nanosleep(&(struct timespec){.tv_sec = -1, .tv_nsec = 0}, 0) != -1 ||
      errno != EINVAL)
    return 8;

  /* The kernel validates pointer accessibility and uses standard EFAULT errors. */
  errno = 0;
  if (clock_gettime(CLOCK_MONOTONIC, 0) != -1 || errno != EFAULT)
    return 9;
  errno = 0;
  if (nanosleep(0, 0) != -1 || errno != EFAULT)
    return 10;

  /*
   * gettimeofday reports epoch seconds and a microsecond fraction. Compare it
   * with CLOCK_REALTIME while allowing the wall clock to cross a second edge.
   */
  struct timeval wall_time;
  if (gettimeofday(&wall_time, 0) != 0 || wall_time.tv_sec < 1000000000L ||
      wall_time.tv_usec < 0 || wall_time.tv_usec >= 1000000L)
    return 12;
  if (clock_gettime(CLOCK_REALTIME, &realtime) != 0)
    return 13;
  long difference = wall_time.tv_sec - realtime.tv_sec;
  if (difference < -1 || difference > 1)
    return 14;

  /* Both output pointers are optional in Linux's legacy gettimeofday syscall. */
  if (gettimeofday(0, 0) != 0)
    return 15;

  errno = 0;
  if (gettimeofday((struct timeval *)1, 0) != -1 || errno != EFAULT)
    return 16;

  return 0;
}
