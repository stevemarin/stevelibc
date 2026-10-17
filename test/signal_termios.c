#include "../include/errno.h"
#include "../include/fcntl.h"
#include "../include/signal.h"
#include "../include/stdint.h"
#include "../include/stdlib.h"
#include "../include/sys/wait.h"
#include "../include/termios.h"
#include "../include/unistd.h"

static volatile sig_atomic_t handled_signal;
static volatile sig_atomic_t handler_masked_signal;
static volatile sig_atomic_t handler_explicitly_masked;
static volatile sig_atomic_t info_signal;
static volatile sig_atomic_t alternate_stack_used;
static char alternate_storage[SIGSTKSZ];

static void test_handler(int signum) { handled_signal = signum; }

static void masking_handler(int signum) {
  sigset_t current_mask;
  handled_signal = signum;
  if (sigprocmask(SIG_SETMASK, 0, &current_mask) == 0) {
    handler_masked_signal = sigismember(&current_mask, signum);
    handler_explicitly_masked = sigismember(&current_mask, SIGUSR2);
  }
}

static void info_handler(int signum, siginfo_t *info, void *context) {
  (void)context;
  info_signal = signum;
  if (info) {
    /* Linux siginfo_t starts with si_signo; the public type is opaque here. */
    const int *signo_field = (const int *)info;
    if (*signo_field == signum)
      info_signal = *signo_field;
  }
}

static void alternate_stack_handler(int signum) {
  volatile char stack_marker = 0;
  (void)signum;
  uintptr_t address = (uintptr_t)&stack_marker;
  uintptr_t start = (uintptr_t)alternate_storage;
  if (address >= start && address < start + sizeof(alternate_storage))
    alternate_stack_used = 1;
}

static int test_signal_sets_and_handlers(void) {
  sigset_t set;
  sigset_t pending;
  if (sigemptyset(&set) != 0 || set != 0 || sigemptyset(0) != -1 || errno != EINVAL || sigaddset(&set, SIGUSR1) != 0 ||
      sigismember(&set, SIGUSR1) != 1 || sigismember(&set, SIGUSR2) != 0 || sigdelset(&set, SIGUSR1) != 0 ||
      sigismember(&set, SIGUSR1) != 0)
    return 1;
  if (sigfillset(&set) != 0 || sigismember(&set, SIGRTMAX) != 1 || sigaddset(&set, 0) != -1 || errno != EINVAL)
    return 2;

  struct sigaction action = {
      .sa_handler = test_handler,
      .sa_flags = SA_RESTART,
      .sa_mask = 0,
  };
  struct sigaction previous;
  if (sigaction(SIGINT, &action, &previous) != 0 || previous.sa_handler != SIG_DFL || sigaction(0, &action, 0) != -1 ||
      errno != EINVAL)
    return 3;

  if (signal(SIGUSR1, test_handler) != SIG_DFL)
    return 4;
  if (raise(SIGUSR1) != 0 || handled_signal != SIGUSR1)
    return 5;
  if (raise(0) != -1 || errno != EINVAL)
    return 6;

  /* The top bit of the kernel's 64-bit sigset carries signal 64. */
  sigset_t realtime_set;
  if (sigemptyset(&realtime_set) != 0 || sigaddset(&realtime_set, SIGRTMAX) != 0 ||
      sigprocmask(SIG_BLOCK, &realtime_set, 0) != 0 || raise(SIGRTMAX) != 0)
    return 23;
  if (sigpending(&pending) != 0 || sigismember(&pending, SIGRTMAX) != 1)
    return 24;
  int realtime_signal = 0;
  if (sigwait(&realtime_set, &realtime_signal) != 0 || realtime_signal != SIGRTMAX)
    return 25;
  if (sigprocmask(SIG_UNBLOCK, &realtime_set, 0) != 0)
    return 26;

  /* A blocked signal becomes pending and is delivered when the mask opens. */
  sigset_t blocked;
  sigset_t original_mask;
  if (sigemptyset(&blocked) != 0 || sigaddset(&blocked, SIGUSR1) != 0 ||
      sigprocmask(SIG_BLOCK, &blocked, &original_mask) != 0)
    return 7;
  errno = 0;
  if (sigprocmask(99, &blocked, 0) != -1 || errno != EINVAL)
    return 22;
  handled_signal = 0;
  if (raise(SIGUSR1) != 0)
    return 8;
  if (sigpending(&pending) != 0 || sigismember(&pending, SIGUSR1) != 1)
    return 9;
  if (sigprocmask(SIG_UNBLOCK, &blocked, 0) != 0 || handled_signal != SIGUSR1)
    return 10;

  /*
   * The delivered signal is automatically masked during its handler unless
   * SA_NODEFER was requested. Querying the mask inside the handler checks the
   * kernel's delivery behavior rather than just the libc bookkeeping.
   */
  struct sigaction masking_action = {
      .sa_handler = masking_handler,
      .sa_flags = 0,
      .sa_mask = 0,
  };
  if (sigaction(SIGUSR1, &masking_action, 0) != 0)
    return 27;
  handler_masked_signal = -1;
  if (raise(SIGUSR1) != 0 || handler_masked_signal != 1)
    return 28;
  if (sigemptyset(&masking_action.sa_mask) != 0 || sigaddset(&masking_action.sa_mask, SIGUSR2) != 0 ||
      sigaction(SIGUSR1, &masking_action, 0) != 0)
    return 36;
  handler_explicitly_masked = 0;
  if (raise(SIGUSR1) != 0 || handler_explicitly_masked != 1)
    return 37;
  masking_action.sa_flags = SA_NODEFER;
  masking_action.sa_mask = 0;
  if (sigaction(SIGUSR1, &masking_action, 0) != 0)
    return 38;
  handler_masked_signal = -1;
  if (raise(SIGUSR1) != 0 || handler_masked_signal != 0)
    return 39;

  struct sigaction info_action = {
      .sa_sigaction = info_handler,
      .sa_flags = SA_SIGINFO,
      .sa_mask = 0,
  };
  if (sigaction(SIGUSR1, &info_action, 0) != 0)
    return 29;
  info_signal = 0;
  if (raise(SIGUSR1) != 0 || info_signal != SIGUSR1)
    return 30;

  /* SA_RESETHAND is a one-shot action; verify reset in a disposable child. */
  struct sigaction one_shot_action = {
      .sa_handler = test_handler,
      .sa_flags = SA_RESETHAND,
      .sa_mask = 0,
  };
  int child = fork();
  if (child < 0)
    return 40;
  if (child == 0) {
    struct sigaction current_action;
    if (sigaction(SIGTERM, &one_shot_action, 0) != 0 || raise(SIGTERM) != 0 ||
        sigaction(SIGTERM, 0, &current_action) != 0 || current_action.sa_handler != SIG_DFL)
      _exit(91);
    _exit(0);
  }
  int status = 0;
  if (waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
    return 41;

  /* sigwait consumes a blocked signal and reports errors as return values. */
  sigset_t wait_set;
  if (sigemptyset(&wait_set) != 0 || sigaddset(&wait_set, SIGUSR2) != 0 || sigprocmask(SIG_BLOCK, &wait_set, 0) != 0 ||
      raise(SIGUSR2) != 0)
    return 11;
  int received_signal = 0;
  errno = EIO;
  if (sigwait(&wait_set, &received_signal) != 0 || received_signal != SIGUSR2 || errno != EIO)
    return 12;
  if (sigprocmask(SIG_SETMASK, &original_mask, 0) != 0)
    return 13;

  /* The alternate stack is per-thread state and can be queried or disabled. */
  stack_t alternate = {
      .ss_sp = alternate_storage,
      .ss_flags = 0,
      .ss_size = sizeof(alternate_storage),
  };
  stack_t previous_stack;
  if (sigaltstack(&alternate, &previous_stack) != 0 || previous_stack.ss_flags & SS_ONSTACK)
    return 14;
  struct sigaction alternate_action = {
      .sa_handler = alternate_stack_handler,
      .sa_flags = SA_ONSTACK,
      .sa_mask = 0,
  };
  if (sigaction(SIGUSR1, &alternate_action, 0) != 0)
    return 31;
  alternate_stack_used = 0;
  if (raise(SIGUSR1) != 0 || alternate_stack_used != 1)
    return 32;
  stack_t current_stack;
  if (sigaltstack(0, &current_stack) != 0 || current_stack.ss_flags != 0)
    return 33;
  stack_t disabled = {.ss_sp = 0, .ss_flags = SS_DISABLE, .ss_size = 0};
  if (sigaltstack(&disabled, 0) != 0)
    return 15;
  stack_t undersized_stack = {
      .ss_sp = alternate_storage,
      .ss_flags = 0,
      .ss_size = MINSIGSTKSZ - 1,
  };
  errno = 0;
  if (sigaltstack(&undersized_stack, 0) != -1 || errno != ENOMEM)
    return 34;
  stack_t bad_stack = {.ss_sp = alternate_storage, .ss_flags = 0x4000, .ss_size = sizeof(alternate_storage)};
  errno = 0;
  if (sigaltstack(&bad_stack, 0) != -1 || errno != EINVAL)
    return 35;

  /*
   * Keep SIGUSR2 blocked until the child has sent it. sigsuspend atomically
   * installs a mask that permits delivery while it sleeps, avoiding a race.
   */
  if (sigaction(SIGUSR2, &action, 0) != 0 || sigprocmask(SIG_BLOCK, &wait_set, 0) != 0)
    return 16;
  child = fork();
  if (child < 0)
    return 17;
  if (child == 0) {
    if (kill(getppid(), SIGUSR2) != 0)
      _exit(90);
    _exit(0);
  }
  sigset_t suspend_mask;
  if (sigemptyset(&suspend_mask) != 0) {
    kill(child, SIGKILL);
    return 18;
  }
  handled_signal = 0;
  errno = 0;
  if (sigsuspend(&suspend_mask) != -1 || errno != EINTR || handled_signal != SIGUSR2)
    return 19;
  if (sigprocmask(SIG_SETMASK, &original_mask, 0) != 0)
    return 20;
  status = 0;
  if (waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
    return 21;
  return 0;
}

static int test_terminal_controls(void) {
  /*
   * /dev/ptmx is a real kernel terminal endpoint, so this exercises successful
   * ioctl translation as well as the error paths tested with ordinary pipes.
   */
  int terminal = open("/dev/ptmx", O_RDWR | O_NOCTTY);
  if (terminal < 0)
    return 30;
  if (!isatty(terminal))
    return 31;

  errno = 0;
  if (tcgetattr(terminal, 0) != -1 || errno != EFAULT)
    return 51;
  errno = 0;
  if (tcsetattr(terminal, TCSANOW, 0) != -1 || errno != EFAULT)
    return 52;

  struct termios original;
  if (tcgetattr(terminal, &original) != 0)
    return 32;
  if (cfgetospeed(&original) != (original.c_cflag & CBAUD) ||
      cfgetispeed(&original) !=
          ((original.c_cflag & CIBAUD) >> 16 ? (original.c_cflag & CIBAUD) >> 16 : original.c_cflag & CBAUD))
    return 33;
  for (unsigned long i = 19; i < NCCS; i++) {
    if (original.c_cc[i] != 0)
      return 34;
  }

  struct termios changed = original;
  changed.c_lflag ^= ECHO;
  changed.c_cc[VMIN] = 2;
  changed.c_cc[VTIME] = 3;
  if (cfsetospeed(&changed, B9600) != 0 || cfgetospeed(&changed) != B9600 || cfsetispeed(&changed, B4800) != 0 ||
      cfgetispeed(&changed) != B4800 || tcsetattr(terminal, TCSANOW, &changed) != 0)
    return 35;
  struct termios observed;
  if (tcgetattr(terminal, &observed) != 0 || observed.c_lflag != changed.c_lflag || observed.c_cc[VMIN] != 2 ||
      observed.c_cc[VTIME] != 3 || cfgetospeed(&observed) != B9600 || cfgetispeed(&observed) != B4800)
    return 36;
  errno = 0;
  if (cfsetospeed(&changed, BOTHER) != -1 || errno != EINVAL)
    return 50;
  errno = 0;
  if (cfgetospeed(0) != (speed_t)-1 || errno != EINVAL)
    return 53;
  errno = 0;
  if (cfgetispeed(0) != (speed_t)-1 || errno != EINVAL)
    return 54;
  errno = 0;
  if (cfsetospeed(0, B9600) != -1 || errno != EINVAL)
    return 55;
  errno = 0;
  if (cfsetispeed(&changed, BOTHER) != -1 || errno != EINVAL)
    return 56;
  if (cfsetispeed(&changed, B4000000) != 0 || cfgetispeed(&changed) != B4000000 ||
      cfsetospeed(&changed, B0) != 0 || cfgetospeed(&changed) != B0)
    return 57;

  /* The public 32-slot struct is translated; only the kernel's 19 slots travel. */
  changed.c_cc[31] = 0x5a;
  if (tcsetattr(terminal, TCSADRAIN, &changed) != 0 || tcsetattr(terminal, TCSAFLUSH, &changed) != 0 ||
      tcgetattr(terminal, &observed) != 0 || observed.c_cc[31] != 0)
    return 37;
  errno = 0;
  if (tcsetattr(terminal, 99, &changed) != -1 || errno != EINVAL)
    return 38;
  errno = 0;
  if (tcflush(terminal, 99) != -1 || errno != EINVAL)
    return 39;
  errno = 0;
  if (tcflow(terminal, 99) != -1 || errno != EINVAL)
    return 40;
  errno = 0;
  if (tcsendbreak(terminal, -1) != -1 || errno != EINVAL)
    return 41;

  if (tcdrain(terminal) != 0 || tcflush(terminal, TCIOFLUSH) != 0 || tcflow(terminal, TCOOFF) != 0 ||
      tcflow(terminal, TCOON) != 0)
    return 46;
  if (tcsendbreak(terminal, 0) != 0 || tcsendbreak(terminal, 1) != 0)
    return 58;

  pid_t terminal_group = tcgetpgrp(terminal);
  if (terminal_group < 0)
    return 59;
  errno = 0;
  int set_group_result = tcsetpgrp(terminal, getpid());
  if (set_group_result != 0 && (set_group_result != -1 || errno == 0))
    return 60;

  struct winsize original_size;
  struct winsize changed_size = {.ws_row = 31, .ws_col = 97, .ws_xpixel = 640, .ws_ypixel = 480};
  if (ioctl(terminal, TIOCGWINSZ, &original_size) != 0 || ioctl(terminal, TIOCSWINSZ, &changed_size) != 0)
    return 47;
  struct winsize observed_size;
  if (ioctl(terminal, TIOCGWINSZ, &observed_size) != 0 || observed_size.ws_row != changed_size.ws_row ||
      observed_size.ws_col != changed_size.ws_col || ioctl(terminal, TIOCSWINSZ, &original_size) != 0)
    return 48;

  struct termios raw = original;
  cfmakeraw(0);
  cfmakeraw(&raw);
  if ((raw.c_lflag & (ECHO | ICANON | ISIG | IEXTEN)) != 0 || (raw.c_cflag & CSIZE) != CS8 || raw.c_cc[VMIN] != 1 ||
      raw.c_cc[VTIME] != 0)
    return 42;
  if (tcsetattr(terminal, TCSANOW, &original) != 0 || close(terminal) != 0)
    return 43;

  errno = 0;
  struct termios invalid;
  if (tcgetattr(-1, &invalid) != -1 || errno != EBADF)
    return 44;
  errno = 0;
  if (isatty(-1) || errno != EBADF)
    return 45;
  errno = 0;
  if (tcgetattr(terminal, &invalid) != -1 || errno != EBADF)
    return 49;
  return 0;
}

int main(void) {
  int result = test_signal_sets_and_handlers();
  if (result)
    return result;
  return test_terminal_controls();
}
