#include "../include/errno.h"
#include "../include/threads.h"

#define ITERATIONS 128

typedef struct {
  int value;
  int start;
  int done;
} worker_data;

static int return_argument(void *arg) { return (int)(long)arg; }

static int finish_detached(void *arg) {
  worker_data *data = (worker_data *)arg;
  while (!__atomic_load_n(&data->start, __ATOMIC_ACQUIRE))
    __asm__ volatile("pause");
  int result = data->value;
  __atomic_store_n(&data->done, 1, __ATOMIC_RELEASE);
  return result;
}

/*
 * Repeated joins exercise blocking, return-value delivery, and stack/TLS
 * reclamation. Detached workers exercise the child-side stack cleanup path.
 */
int main(void) {
  if (thr_create(0, 0) != -1 || errno != EINVAL)
    return 1;
  if (thr_join(-1, 0) != -1 || errno != ESRCH)
    return 2;
  if (thr_detach(-1) != -1 || errno != ESRCH)
    return 11;

  for (int i = 0; i < ITERATIONS; i++) {
    int tid = thr_create(return_argument, (void *)(long)(i + 17));
    if (tid <= 0)
      return 3;
    int result = -1;
    if (thr_join(tid, &result) != 0 || result != i + 17)
      return 4;
    if (thr_join(tid, 0) != -1 || errno != ESRCH)
      return 5;
  }
  worker_data detached = {.value = 73, .start = 0, .done = 0};
  int tid = thr_create(finish_detached, &detached);
  if (tid <= 0 || thr_detach(tid) != 0)
    return 6;
  if (thr_join(tid, 0) != -1 || errno != EINVAL || thr_detach(tid) != -1 || errno != EINVAL)
    return 9;
  __atomic_store_n(&detached.start, 1, __ATOMIC_RELEASE);
  while (!__atomic_load_n(&detached.done, __ATOMIC_ACQUIRE))
    __asm__ volatile("pause");

  /* The worker may still be in its final exit sequence; detached handles cannot join. */
  if (thr_join(tid, 0) != -1)
    return 7;

  for (int i = 0; i < ITERATIONS; i++) {
    worker_data data = {.value = i, .start = 0, .done = 0};
    tid = thr_create(finish_detached, &data);
    if (tid <= 0 || thr_detach(tid) != 0)
      return 8;
    __atomic_store_n(&data.start, 1, __ATOMIC_RELEASE);
    while (!__atomic_load_n(&data.done, __ATOMIC_ACQUIRE))
      __asm__ volatile("pause");
    /*
     * Keep the worker's argument alive until it has announced completion;
     * detached cleanup no longer accesses it afterward.
     */
  }

  return 0;
}
