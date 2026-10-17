#ifndef SIGNAL_H
#define SIGNAL_H

#include "./sys/types.h"

/*
 * Linux x86_64 signal numbers. Real-time signals occupy 34 through 64;
 * signal-set helpers below represent these values as bits in a 64-bit word.
 */
#define SIGHUP 1
#define SIGINT 2
#define SIGQUIT 3
#define SIGILL 4
#define SIGTRAP 5
#define SIGABRT 6
#define SIGBUS 7
#define SIGFPE 8
#define SIGKILL 9
#define SIGUSR1 10
#define SIGSEGV 11
#define SIGUSR2 12
#define SIGPIPE 13
#define SIGALRM 14
#define SIGTERM 15
#define SIGSTKFLT 16
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19
#define SIGTSTP 20
#define SIGTTIN 21
#define SIGTTOU 22
#define SIGURG 23
#define SIGXCPU 24
#define SIGXFSZ 25
#define SIGVTALRM 26
#define SIGPROF 27
#define SIGWINCH 28
#define SIGIO 29
#define SIGPWR 30
#define SIGSYS 31
#define SIGRTMIN 34
#define SIGRTMAX 64

#define SIG_DFL ((void (*)(int))0)
#define SIG_IGN ((void (*)(int))1)
#define SIG_ERR ((void (*)(int)) - 1)

#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

typedef unsigned long sigset_t;
typedef int sig_atomic_t;
typedef struct siginfo siginfo_t;

/*
 * Signal disposition passed to rt_sigaction. The anonymous union selects
 * either a one-argument handler or the SA_SIGINFO form. This layout follows
 * Linux x86_64; libc supplies the architecture-specific restorer trampoline.
 */
struct sigaction {
  union {
    void (*sa_handler)(int); /* Basic handler, SIG_DFL, or SIG_IGN */
    void (*sa_sigaction)(int, siginfo_t *, void *); /* Extended SA_SIGINFO form */
  };
  unsigned long sa_flags; /* SA_* behavior modifiers */
  void (*sa_restorer)(void); /* Kernel return trampoline, installed by libc */
  sigset_t sa_mask; /* Signals blocked while this handler runs */
};

/* Alternate signal stack used when an action includes SA_ONSTACK. */
typedef struct {
  void *ss_sp; /* Base address of caller-provided storage */
  int ss_flags; /* SS_DISABLE or kernel-reported SS_ONSTACK state */
  size_t ss_size; /* Usable storage size; must meet MINSIGSTKSZ when enabled */
} stack_t;

#define SA_NOCLDSTOP 0x00000001UL
#define SA_NOCLDWAIT 0x00000002UL
#define SA_SIGINFO 0x00000004UL
#define SA_RESTORER 0x04000000UL
#define SA_ONSTACK 0x08000000UL
#define SA_RESTART 0x10000000UL
#define SA_NODEFER 0x40000000UL
#define SA_RESETHAND 0x80000000UL

#define SS_ONSTACK 1
#define SS_DISABLE 2
#define MINSIGSTKSZ 2048
#define SIGSTKSZ 8192

/*
 * Signal dispositions are process-wide; the blocked/pending masks and
 * alternate stack are per-thread. These operations cross into the kernel.
 */
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
int sigpending(sigset_t *set);
int sigsuspend(const sigset_t *mask);
int sigaltstack(const stack_t *stack, stack_t *old_stack);

/*
 * Signal sets are local bitmasks: bit (signum - 1) represents a signal.
 * Editing a set does not itself block, unblock, or deliver anything.
 */
int sigemptyset(sigset_t *set);
int sigfillset(sigset_t *set);
int sigaddset(sigset_t *set, int signum);
int sigdelset(sigset_t *set, int signum);
int sigismember(const sigset_t *set, int signum);

/*
 * Convenience delivery/wait APIs. sigwait requires its signals to be blocked
 * in the calling thread and returns an error number directly on failure.
 */
void (*signal(int signum, void (*handler)(int)))(int);
int raise(int signum);
int sigwait(const sigset_t *set, int *signum);
int sigwaitinfo(const sigset_t *set, siginfo_t *info);

#endif
