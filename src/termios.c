#include "../include/termios.h"
#include "../include/errno.h"
#include "../include/syscall.h"

/*
 * TCGETS and TCSETS use the kernel's compact termios, not libc's public
 * structure. The kernel structure has no speed fields and only 19 c_cc slots;
 * libc exposes 32 slots and derives speeds from c_cflag.
 */
/* Linux's ioctl ABI has fewer control characters than the public libc struct. */
enum { KERNEL_NCCS = 19 };
struct kernel_termios {
  tcflag_t c_iflag; /* Input processing flags */
  tcflag_t c_oflag; /* Output processing flags */
  tcflag_t c_cflag; /* Control and baud-rate flags */
  tcflag_t c_lflag; /* Local line-discipline flags */
  cc_t c_line; /* Kernel line discipline */
  cc_t c_cc[KERNEL_NCCS]; /* Control characters physically carried by ioctl */
};

_Static_assert(__builtin_offsetof(struct kernel_termios, c_line) == 16, "Linux x86_64 kernel termios c_line offset");
_Static_assert(__builtin_offsetof(struct kernel_termios, c_cc) == 17,
               "Linux x86_64 kernel termios control-character offset");
_Static_assert(sizeof(struct kernel_termios) == 36, "Linux x86_64 kernel termios size");

static int ioctl_value(int fd, unsigned long request, long value) {
  return (int)__syscall_result(syscall3(SYS_ioctl, fd, request, value));
}

int ioctl(int fd, unsigned long request, ...) {
  __builtin_va_list args;
  __builtin_va_start(args, request);
  void *argument = __builtin_va_arg(args, void *);
  __builtin_va_end(args);
  return ioctl_value(fd, request, (long)argument);
}

int tcgetattr(int fd, struct termios *termios_p) {
  if (!termios_p) {
    errno = EFAULT;
    return -1;
  }

  struct kernel_termios kernel_attributes;
  if (ioctl_value(fd, TCGETS, (long)&kernel_attributes) < 0)
    return -1;

  /* Clear the public-only tail so unsupported kernel slots are deterministic. */
  for (unsigned long i = 0; i < sizeof(*termios_p); i++)
    ((unsigned char *)termios_p)[i] = 0;
  termios_p->c_iflag = kernel_attributes.c_iflag;
  termios_p->c_oflag = kernel_attributes.c_oflag;
  termios_p->c_cflag = kernel_attributes.c_cflag;
  termios_p->c_lflag = kernel_attributes.c_lflag;
  termios_p->c_line = kernel_attributes.c_line;
  for (unsigned long i = 0; i < KERNEL_NCCS; i++)
    termios_p->c_cc[i] = kernel_attributes.c_cc[i];

  /* On Linux, an unset input baud field means “same as the output speed.” */
  termios_p->c_ospeed = termios_p->c_cflag & CBAUD;
  termios_p->c_ispeed = (termios_p->c_cflag & CIBAUD) >> 16;
  if (!termios_p->c_ispeed)
    termios_p->c_ispeed = termios_p->c_ospeed;
  return 0;
}

int tcsetattr(int fd, int optional_actions, const struct termios *termios_p) {
  if (!termios_p) {
    errno = EFAULT;
    return -1;
  }

  unsigned long request;
  switch (optional_actions) {
  case TCSANOW:
    request = TCSETS;
    break;
  case TCSADRAIN:
    request = TCSETSW;
    break;
  case TCSAFLUSH:
    request = TCSETSF;
    break;
  default:
    errno = EINVAL;
    return -1;
  }

  struct kernel_termios kernel_attributes = {
      .c_iflag = termios_p->c_iflag,
      .c_oflag = termios_p->c_oflag,
      .c_cflag = termios_p->c_cflag,
      .c_lflag = termios_p->c_lflag,
      .c_line = termios_p->c_line,
  };
  for (unsigned long i = 0; i < KERNEL_NCCS; i++)
    kernel_attributes.c_cc[i] = termios_p->c_cc[i];

  return ioctl_value(fd, request, (long)&kernel_attributes);
}

int tcdrain(int fd) {
  /* TCSBRK with a nonzero argument waits for queued output to drain. */
  return ioctl_value(fd, TCSBRK, 1);
}

int tcflow(int fd, int action) {
  if (action < TCOOFF || action > TCION) {
    errno = EINVAL;
    return -1;
  }
  return ioctl_value(fd, TCXONC, action);
}

int tcflush(int fd, int queue_selector) {
  if (queue_selector < TCIFLUSH || queue_selector > TCIOFLUSH) {
    errno = EINVAL;
    return -1;
  }
  return ioctl_value(fd, TCFLSH, queue_selector);
}

int tcsendbreak(int fd, int duration) {
  if (duration < 0) {
    errno = EINVAL;
    return -1;
  }
  if (duration == 0)
    return ioctl_value(fd, TCSBRK, 0);
  return ioctl_value(fd, TCSBRKP, duration);
}

int tcgetpgrp(int fd) {
  pid_t process_group;
  if (ioctl_value(fd, TIOCGPGRP, (long)&process_group) < 0)
    return -1;
  return process_group;
}

int tcsetpgrp(int fd, pid_t process_group) { return ioctl_value(fd, TIOCSPGRP, (long)&process_group); }

int isatty(int fd) {
  struct termios attributes;
  return tcgetattr(fd, &attributes) == 0;
}

static int valid_speed(speed_t speed) { return speed <= B38400 || (speed >= B57600 && speed <= B4000000); }

speed_t cfgetispeed(const struct termios *termios_p) {
  if (!termios_p) {
    errno = EINVAL;
    return (speed_t)-1;
  }
  speed_t input_speed = (termios_p->c_cflag & CIBAUD) >> 16;
  return input_speed ? input_speed : termios_p->c_cflag & CBAUD;
}

speed_t cfgetospeed(const struct termios *termios_p) {
  if (!termios_p) {
    errno = EINVAL;
    return (speed_t)-1;
  }
  return termios_p->c_cflag & CBAUD;
}

int cfsetispeed(struct termios *termios_p, speed_t speed) {
  if (!termios_p || !valid_speed(speed)) {
    errno = EINVAL;
    return -1;
  }
  termios_p->c_cflag = (termios_p->c_cflag & ~CIBAUD) | ((speed << 16) & CIBAUD);
  termios_p->c_ispeed = speed ? speed : cfgetospeed(termios_p);
  return 0;
}

int cfsetospeed(struct termios *termios_p, speed_t speed) {
  if (!termios_p || !valid_speed(speed)) {
    errno = EINVAL;
    return -1;
  }
  termios_p->c_cflag = (termios_p->c_cflag & ~CBAUD) | (speed & CBAUD);
  termios_p->c_ospeed = speed;
  if (!(termios_p->c_cflag & CIBAUD))
    termios_p->c_ispeed = speed;
  return 0;
}

void cfmakeraw(struct termios *termios_p) {
  if (!termios_p)
    return;
  termios_p->c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
  termios_p->c_oflag &= ~OPOST;
  termios_p->c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  termios_p->c_cflag = (termios_p->c_cflag & ~(CSIZE | PARENB)) | CS8;
  termios_p->c_cc[VMIN] = 1;
  termios_p->c_cc[VTIME] = 0;
}
