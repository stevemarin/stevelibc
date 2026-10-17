#include "../include/threads.h"
#include "../include/stdlib.h"
#include "../include/sys/mman.h"
#include "../include/syscall.h"
#include "../include/tls.h"

/* ==========================================================================
 * THREADS.C : Thread creation, lifecycle, and resource reclamation
 * ========================================================================== */

#define CLONE_VM 0x00000100
#define CLONE_FS 0x00000200
#define CLONE_FILES 0x00000400
#define CLONE_SIGHAND 0x00000800
#define CLONE_PARENT_SETTID 0x00100000
#define CLONE_THREAD 0x00010000
#define CLONE_SETTLS 0x00080000
#define CLONE_CHILD_CLEARTID 0x00200000

#define FUTEX_WAIT 0
#define THREAD_STACK_SIZE (1024UL * 1024UL)

/*
 * Control-record states, protected by thread_lock: a running thread may be
 * joined or detached; completion makes it joinable; joining reserves it
 * against a concurrent join/detach; detached completion reclaims itself.
 */
enum {
  THREAD_RUNNING, /* Child may still execute its user function */
  THREAD_COMPLETE, /* User function returned; parent may join or detach */
  THREAD_DETACHED, /* Child will reclaim its own control record and stack */
  THREAD_JOINING /* One caller owns the join and resource-release operation */
};

extern int __clone_thread(int flags, void *child_stack, void *ptid, void *ctid, void *tls, void *arg);

typedef struct thread_control {
  thread_func_t fn; /* Entry point called by the assembly clone trampoline */
  void *arg; /* User argument passed to fn */
  int result; /* Return value published before completed becomes visible */
  int completed; /* Atomic completion indicator for result readers */
  int state; /* One of THREAD_*; guards join/detach ownership transitions */
  void *stack; /* mmap allocation kept until the thread no longer runs on it */
  tls_control_block *tls; /* Per-thread FS-based errno and TLS state */
  struct thread_slot *slot; /* Long-lived kernel TID storage for clear-child-TID */
  struct thread_control *next; /* Intrusive list link for joinable threads */
} thread_control;

typedef struct thread_slot {
  int tid; /* Kernel-written child TID; cleared to zero when the thread exits */
  int id; /* Published TID used by the public join/detach calls */
  int used; /* 0 free, 1 active, 2 retired until kernel clear-child-TID */
} thread_slot;

/*
 * Handoff from __thread_complete to threads.s. The active thread cannot free
 * its own stack in C, so the trampoline receives this pair and unmaps the
 * stack only after switching away from it.
 */
typedef struct {
  void *stack; /* Mapping to release after leaving the child stack */
  unsigned long stack_size; /* Mapping length required by munmap */
} thread_cleanup;

_Static_assert(sizeof(thread_cleanup) == 16, "thread cleanup must return in two registers");

/* Bounded static TID storage must remain valid through kernel clear-child-TID. */
#define THREAD_SLOT_COUNT 1024

static thread_control *threads;
/*
 * CLONE_CHILD_CLEARTID writes to this static table during kernel thread exit.
 * A detached child may release its heap control record before that write, so
 * the TID word itself must outlive the child. Retired slots are reused only
 * after the kernel has cleared the TID to zero.
 */
static thread_slot thread_slots[THREAD_SLOT_COUNT];
static unsigned char thread_lock;

static void lock_threads(void) {
  while (__atomic_test_and_set(&thread_lock, __ATOMIC_ACQUIRE)) {
    __asm__ volatile("pause");
  }
}

static void unlock_threads(void) { __atomic_clear(&thread_lock, __ATOMIC_RELEASE); }

static thread_control *find_thread(int tid) {
  for (thread_control *thread = threads; thread; thread = thread->next) {
    if (thread->slot->id == tid)
      return thread;
  }
  return 0;
}

static void remove_thread(thread_control *thread) {
  thread_control **link = &threads;
  while (*link && *link != thread)
    link = &(*link)->next;
  if (*link)
    *link = thread->next;
}

static int release_thread(thread_control *thread) {
  void *stack = thread->stack;
  tls_control_block *tls = thread->tls;
  free(tls);
  free(thread);
  return munmap(stack, THREAD_STACK_SIZE);
}

static int wait_for_thread_exit(thread_control *thread) {
  for (;;) {
    int tid = __atomic_load_n(&thread->slot->tid, __ATOMIC_ACQUIRE);
    if (tid == 0)
      return 0;

    /* The kernel clears child_tid and wakes this futex as the thread exits. */
    long result = syscall6(SYS_futex, (long)&thread->slot->tid, FUTEX_WAIT, tid, 0, 0, 0);
    if (result < 0 && result != -EINTR && result != -EAGAIN) {
      __syscall_result(result);
      return -1;
    }
  }
}

/* Called by the assembly trampoline after the user thread function returns. */
thread_cleanup __thread_complete(thread_control *thread, int result) {
  thread_cleanup cleanup = {0, 0};
  lock_threads();
  __atomic_store_n(&thread->result, result, __ATOMIC_RELEASE);
  __atomic_store_n(&thread->completed, 1, __ATOMIC_RELEASE);

  if (thread->state == THREAD_DETACHED) {
    remove_thread(thread);
    cleanup.stack = thread->stack;
    cleanup.stack_size = THREAD_STACK_SIZE;
    tls_control_block *tls = thread->tls;
    thread->slot->used = 2;
    unlock_threads();

    /*
     * Keep the static TID slot for the kernel's clear-child-TID write. After
     * returning, assembly only unmaps the active stack and exits.
     */
    free(tls);
    free(thread);
    return cleanup;
  }

  if (thread->state == THREAD_RUNNING)
    thread->state = THREAD_COMPLETE;
  unlock_threads();
  return cleanup;
}

/*
 * Low-level clone system call wrapper (SYS_clone = 56).
 */
int clone(void *child_stack, int flags, void *arg, void *ptid, void *tls, void *ctid) {
  return (int)__syscall_result(__clone_thread(flags, child_stack, ptid, ctid, tls, arg));
}

int thr_create(thread_func_t fn, void *arg) {
  if (!fn) {
    errno = EINVAL;
    return -1;
  }

  thread_control *thread = (thread_control *)malloc(sizeof(*thread));
  if (!thread) {
    errno = ENOMEM;
    return -1;
  }
  thread->fn = fn;
  thread->arg = arg;
  thread->result = 0;
  thread->completed = 0;
  thread->state = THREAD_RUNNING;
  thread->next = 0;

  void *stack = mmap(0, THREAD_STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (stack == MAP_FAILED) {
    free(thread);
    return -1;
  }
  thread->stack = stack;

  tls_control_block *tls = (tls_control_block *)malloc(sizeof(*tls));
  if (!tls) {
    munmap(stack, THREAD_STACK_SIZE);
    free(thread);
    errno = ENOMEM;
    return -1;
  }
  tls->self = tls;
  tls->errno_value = 0;
  thread->tls = tls;

  void *stack_top = (char *)stack + THREAD_STACK_SIZE;
  stack_top = (void *)((unsigned long)stack_top & ~15UL);

  int flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD | CLONE_SETTLS | CLONE_PARENT_SETTID |
              CLONE_CHILD_CLEARTID;

  /* Publish the record before clone so the child can complete immediately. */
  lock_threads();
  thread_slot *slot = 0;
  for (int i = 0; i < THREAD_SLOT_COUNT; i++) {
    thread_slot *candidate = &thread_slots[i];
    if (candidate->used == 2 && __atomic_load_n(&candidate->tid, __ATOMIC_ACQUIRE) == 0)
      candidate->used = 0;
    if (!candidate->used) {
      candidate->used = 1;
      candidate->tid = 0;
      candidate->id = 0;
      slot = candidate;
      break;
    }
  }
  if (!slot) {
    unlock_threads();
    free(tls);
    munmap(stack, THREAD_STACK_SIZE);
    free(thread);
    errno = EAGAIN;
    return -1;
  }
  thread->slot = slot;
  thread->next = threads;
  threads = thread;
  unlock_threads();

  int result = clone(stack_top, flags, thread, &slot->tid, tls, &slot->tid);
  if (result < 0) {
    lock_threads();
    remove_thread(thread);
    slot->used = 0;
    unlock_threads();
    free(tls);
    munmap(stack, THREAD_STACK_SIZE);
    free(thread);
    return -1;
  }
  lock_threads();
  slot->id = result;
  unlock_threads();
  return result;
}

int thr_join(int tid, int *result) {
  lock_threads();
  thread_control *thread = find_thread(tid);
  if (!thread) {
    unlock_threads();
    errno = ESRCH;
    return -1;
  }
  if (thread->state == THREAD_DETACHED || thread->state == THREAD_JOINING) {
    unlock_threads();
    errno = EINVAL;
    return -1;
  }
  thread->state = THREAD_JOINING;
  unlock_threads();

  if (wait_for_thread_exit(thread) != 0) {
    lock_threads();
    thread->state = __atomic_load_n(&thread->completed, __ATOMIC_ACQUIRE) ? THREAD_COMPLETE : THREAD_RUNNING;
    unlock_threads();
    return -1;
  }

  int thread_result = __atomic_load_n(&thread->result, __ATOMIC_ACQUIRE);
  lock_threads();
  remove_thread(thread);
  thread->slot->used = 0;
  thread->slot->id = 0;
  unlock_threads();

  if (result)
    *result = thread_result;
  return release_thread(thread);
}

int thr_detach(int tid) {
  lock_threads();
  thread_control *thread = find_thread(tid);
  if (!thread) {
    unlock_threads();
    errno = ESRCH;
    return -1;
  }
  if (thread->state == THREAD_DETACHED || thread->state == THREAD_JOINING) {
    unlock_threads();
    errno = EINVAL;
    return -1;
  }

  if (thread->state == THREAD_RUNNING) {
    /* The child notices this state at completion and reclaims its own stack. */
    thread->state = THREAD_DETACHED;
    unlock_threads();
    return 0;
  }

  /* A completed child has stopped using its record, but may not have exited yet. */
  thread->state = THREAD_DETACHED;
  unlock_threads();

  if (wait_for_thread_exit(thread) != 0) {
    lock_threads();
    thread->state = THREAD_COMPLETE;
    unlock_threads();
    return -1;
  }
  lock_threads();
  remove_thread(thread);
  thread->slot->used = 0;
  thread->slot->id = 0;
  unlock_threads();
  return release_thread(thread);
}

int thread_wrapper(void *arg) {
  thread_control *thread = (thread_control *)arg;
  return thread->fn(thread->arg);
}
