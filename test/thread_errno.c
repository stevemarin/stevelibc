#include "../include/errno.h"
#include "../include/threads.h"
#include "../include/unistd.h"

typedef struct {
  int error;
  int *errno_slot;
  int done;
} thread_result;

/*
 * Each worker deliberately fails a syscall, then publishes its thread-local
 * errno and slot address before marking its result complete.
 */
static int fail_read_in_worker(void *arg) {
  thread_result *result = (thread_result *)arg;
  char byte;

  errno = 0;
  if (read(-1, &byte, 1) != -1)
    result->error = -1;
  else
    result->error = errno;
  result->errno_slot = &errno;

  __atomic_store_n(&result->done, 1, __ATOMIC_RELEASE);
  return 0;
}

int main(void) {
  thread_result first = {0, 0, 0};
  thread_result second = {0, 0, 0};
  int *main_errno_slot = &errno;
  int first_tid;
  int second_tid;

  /* Workers must not change either the main thread's errno or its slot. */
  errno = EINTR;

  first_tid = thr_create(fail_read_in_worker, &first);
  if (first_tid <= 0)
    return 1;
  second_tid = thr_create(fail_read_in_worker, &second);
  if (second_tid <= 0) {
    /* Let the already-started worker finish before returning from this test. */
    while (!__atomic_load_n(&first.done, __ATOMIC_ACQUIRE))
      __asm__ volatile("pause");
    if (thr_join(first_tid, 0) != 0)
      return 7;
    return 2;
  }

  while (!__atomic_load_n(&first.done, __ATOMIC_ACQUIRE) || !__atomic_load_n(&second.done, __ATOMIC_ACQUIRE))
    __asm__ volatile("pause");

  if (first.error != EBADF || second.error != EBADF)
    return 3;
  if (!first.errno_slot || !second.errno_slot)
    return 4;
  if (first.errno_slot == second.errno_slot || first.errno_slot == main_errno_slot ||
      second.errno_slot == main_errno_slot)
    return 5;
  if (errno != EINTR)
    return 6;
  int first_result = -1;
  int second_result = -1;
  if (thr_join(first_tid, &first_result) != 0 || thr_join(second_tid, &second_result) != 0)
    return 8;
  if (first_result != 0 || second_result != 0)
    return 9;

  return 0;
}
