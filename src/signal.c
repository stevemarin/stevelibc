#include "../include/signal.h"
#include "../include/errno.h"
#include "../include/syscall.h"

/*
 * rt_sig* syscalls take an explicit signal-set byte count. This target's
 * kernel ABI uses one machine word, even though libc exposes sigset_t itself.
 */
enum { KERNEL_SIGSET_SIZE = sizeof(unsigned long) };

extern void __restore_rt(void);

static int valid_signum(int signum) {
  /* 32 and 33 are reserved by Linux for its threading implementation. */
  return signum > 0 && signum <= SIGRTMAX && signum != 32 && signum != 33;
}

int sigemptyset(sigset_t *set) {
  if (!set) {
    errno = EINVAL;
    return -1;
  }
  *set = 0;
  return 0;
}

int sigfillset(sigset_t *set) {
  if (!set) {
    errno = EINVAL;
    return -1;
  }
  *set = ~0UL;
  return 0;
}

int sigaddset(sigset_t *set, int signum) {
  if (!set || !valid_signum(signum)) {
    errno = EINVAL;
    return -1;
  }
  *set |= 1UL << (signum - 1);
  return 0;
}

int sigdelset(sigset_t *set, int signum) {
  if (!set || !valid_signum(signum)) {
    errno = EINVAL;
    return -1;
  }
  *set &= ~(1UL << (signum - 1));
  return 0;
}

int sigismember(const sigset_t *set, int signum) {
  if (!set || !valid_signum(signum)) {
    errno = EINVAL;
    return -1;
  }
  return (*set & (1UL << (signum - 1))) != 0;
}

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
  struct sigaction kernel_act;
  struct sigaction kernel_oldact;

  if (act) {
    kernel_act = *act;
    if (kernel_act.sa_handler != SIG_DFL && kernel_act.sa_handler != SIG_IGN) {
      /*
       * A handler returns through this trampoline rather than directly into
       * interrupted code; rt_sigreturn asks the kernel to restore that state.
       */
      kernel_act.sa_restorer = __restore_rt;
      kernel_act.sa_flags |= SA_RESTORER;
    }
  }

  long result = __syscall_result(syscall4(SYS_rt_sigaction, signum, act ? (long)&kernel_act : 0,
                                          oldact ? (long)&kernel_oldact : 0, KERNEL_SIGSET_SIZE));
  if (result < 0)
    return -1;
  if (oldact)
    *oldact = kernel_oldact;
  return 0;
}

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
  return (int)__syscall_result(syscall4(SYS_rt_sigprocmask, how, (long)set, (long)oldset, KERNEL_SIGSET_SIZE));
}

int sigpending(sigset_t *set) {
  return (int)__syscall_result(syscall2(SYS_rt_sigpending, (long)set, KERNEL_SIGSET_SIZE));
}

int sigsuspend(const sigset_t *mask) {
  return (int)__syscall_result(syscall2(SYS_rt_sigsuspend, (long)mask, KERNEL_SIGSET_SIZE));
}

int sigaltstack(const stack_t *stack, stack_t *old_stack) {
  return (int)__syscall_result(syscall2(SYS_sigaltstack, (long)stack, (long)old_stack));
}

/*
 * The traditional signal() wrapper uses restart-after-handler behavior.
 * Explicit sigaction remains available when a caller needs finer control.
 */
void (*signal(int signum, void (*handler)(int)))(int) {
  struct sigaction action = {
      .sa_handler = handler,
      .sa_flags = SA_RESTART,
      .sa_mask = 0,
  };
  struct sigaction previous;
  if (sigaction(signum, &action, &previous) < 0)
    return SIG_ERR;
  return previous.sa_handler;
}

int raise(int signum) {
  if (!valid_signum(signum)) {
    errno = EINVAL;
    return -1;
  }
  return (int)__syscall_result(syscall3(SYS_tgkill, syscall0(SYS_getpid), syscall0(SYS_gettid), signum));
}

int sigwaitinfo(const sigset_t *set, siginfo_t *info) {
  /* A null timeout waits until one of the already-blocked signals arrives. */
  return (int)__syscall_result(syscall4(SYS_rt_sigtimedwait, (long)set, (long)info, 0, KERNEL_SIGSET_SIZE));
}

int sigwait(const sigset_t *set, int *signum) {
  if (!set || !signum)
    return EINVAL;

  /* POSIX sigwait returns an error number directly instead of setting errno. */
  int previous_errno = errno;
  int received = sigwaitinfo(set, 0);
  if (received < 0) {
    int error = errno;
    errno = previous_errno;
    return error;
  }
  *signum = received;
  errno = previous_errno;
  return 0;
}
