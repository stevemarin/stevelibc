#ifndef THREADS_H
#define THREADS_H

typedef int (*thread_func_t)(void *);

/*
 * Low-level Linux clone interface. The child starts on child_stack with arg
 * passed to the assembly trampoline; ptid/ctid and tls correspond to the
 * kernel's parent-TID, child-TID, and thread-local-storage arguments.
 */
int clone(void *child_stack, int flags, void *arg, void *ptid, void *tls, void *ctid);
/* Create a joinable thread; fn's integer return value is collected by thr_join. */
int thr_create(thread_func_t fn, void *arg);
/*
 * Wait for a joinable thread by its returned TID. At most one caller may join;
 * result may be null. Join/detach return 0 on success and -1 on error.
 */
int thr_join(int tid, int *result);
/* A detached thread reclaims its stack and TLS when it exits and cannot be joined. */
int thr_detach(int tid);

#endif
