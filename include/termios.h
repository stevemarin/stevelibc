#ifndef TERMIOS_H
#define TERMIOS_H

#include "./sys/types.h"

typedef unsigned int tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;

/*
 * POSIX/Linux libc exposes 32 control-character slots, but the x86_64 Linux
 * TCGETS/TCSETS ioctl ABI carries only c_line and the first 19 characters.
 * termios.c translates between this public structure and the shorter kernel
 * structure instead of passing this memory directly to ioctl.
 */
#define NCCS 32
struct termios {
  tcflag_t c_iflag; /* Input transformations (for example CR/LF and flow control) */
  tcflag_t c_oflag; /* Output transformations applied before bytes reach the device */
  tcflag_t c_cflag; /* Hardware framing, modem control, and encoded baud rates */
  tcflag_t c_lflag; /* Local line discipline: canonical mode, echo, and signals */
  cc_t c_line; /* Kernel line-discipline selector */
  cc_t c_cc[NCCS]; /* Per-terminal control characters indexed by V* constants */
  speed_t c_ispeed; /* Decoded input speed; also represented in c_cflag */
  speed_t c_ospeed; /* Decoded output speed; also represented in c_cflag */
};

/* Input flags: transformations and flow-control behavior on received bytes. */
#define IGNBRK 0000001
#define BRKINT 0000002
#define IGNPAR 0000004
#define PARMRK 0000010
#define INPCK 0000020
#define ISTRIP 0000040
#define INLCR 0000100
#define IGNCR 0000200
#define ICRNL 0000400
#define IUCLC 0001000
#define IXON 0002000
#define IXANY 0004000
#define IXOFF 0010000
#define IMAXBEL 0020000
#define IUTF8 0040000

/* Output flags: post-processing plus masks and values for device delays. */
#define OPOST 0000001
#define OLCUC 0000002
#define ONLCR 0000004
#define OCRNL 0000010
#define ONOCR 0000020
#define ONLRET 0000040
#define OFILL 0000100
#define OFDEL 0000200
#define NLDLY 0000400
#define NL0 0000000
#define NL1 0000400
#define CRDLY 0003000
#define CR0 0000000
#define CR1 0001000
#define CR2 0002000
#define CR3 0003000
#define TABDLY 0014000
#define TAB0 0000000
#define TAB1 0004000
#define TAB2 0010000
#define TAB3 0014000
#define XTABS TAB3
#define BSDLY 0020000
#define BS0 0000000
#define BS1 0020000
#define VTDLY 0040000
#define VT0 0000000
#define VT1 0040000
#define FFDLY 0100000
#define FF0 0000000
#define FF1 0100000

/* Control flags. Baud-rate values are encoded in c_cflag on Linux. */
#define CBAUD 0x0000100f
#define CBAUDEX 0x00001000
#define CIBAUD 0x100f0000
#define CSIZE 0x00000030
#define CS5 0x00000000
#define CS6 0x00000010
#define CS7 0x00000020
#define CS8 0x00000030
#define CSTOPB 0x00000040
#define CREAD 0x00000080
#define PARENB 0x00000100
#define PARODD 0x00000200
#define HUPCL 0x00000400
#define CLOCAL 0x00000800
#define CMSPAR 0x40000000
#define CRTSCTS 0x80000000
#define BOTHER 0x00001000
#define B0 0x00000000
#define B50 0x00000001
#define B75 0x00000002
#define B110 0x00000003
#define B134 0x00000004
#define B150 0x00000005
#define B200 0x00000006
#define B300 0x00000007
#define B600 0x00000008
#define B1200 0x00000009
#define B1800 0x0000000a
#define B2400 0x0000000b
#define B4800 0x0000000c
#define B9600 0x0000000d
#define B19200 0x0000000e
#define B38400 0x0000000f
#define B57600 0x00001001
#define B115200 0x00001002
#define B230400 0x00001003
#define B460800 0x00001004
#define B500000 0x00001005
#define B576000 0x00001006
#define B921600 0x00001007
#define B1000000 0x00001008
#define B1152000 0x00001009
#define B1500000 0x0000100a
#define B2000000 0x0000100b
#define B2500000 0x0000100c
#define B3000000 0x0000100d
#define B3500000 0x0000100e
#define B4000000 0x0000100f
#define EXTA B19200
#define EXTB B38400

/* Local flags: terminal line-discipline behavior visible to the application. */
#define ISIG 0x00001
#define ICANON 0x00002
#define XCASE 0x00004
#define ECHO 0x00008
#define ECHOE 0x00010
#define ECHOK 0x00020
#define ECHONL 0x00040
#define NOFLSH 0x00080
#define TOSTOP 0x00100
#define ECHOCTL 0x00200
#define ECHOPRT 0x00400
#define ECHOKE 0x00800
#define FLUSHO 0x01000
#define PENDIN 0x04000
#define IEXTEN 0x08000
#define EXTPROC 0x10000

/* Indices into c_cc; the active termios flags determine each character's role. */
#define VINTR 0
#define VQUIT 1
#define VERASE 2
#define VKILL 3
#define VEOF 4
#define VTIME 5
#define VMIN 6
#define VSWTC 7
#define VSTART 8
#define VSTOP 9
#define VSUSP 10
#define VEOL 11
#define VREPRINT 12
#define VDISCARD 13
#define VWERASE 14
#define VLNEXT 15
#define VEOL2 16

/* When tcsetattr applies changes: immediately, after output drains, or after flush. */
#define TCSANOW 0
#define TCSADRAIN 1
#define TCSAFLUSH 2

/* Linux ioctl request numbers consumed by ioctl/tc* wrappers below. */
#define TCGETS 0x5401
#define TCSETS 0x5402
#define TCSETSW 0x5403
#define TCSETSF 0x5404
#define TCSBRK 0x5409
#define TCXONC 0x540a
#define TCFLSH 0x540b
#define TIOCEXCL 0x540c
#define TIOCNXCL 0x540d
#define TIOCSCTTY 0x540e
#define TIOCGPGRP 0x540f
#define TIOCSPGRP 0x5410
#define TIOCOUTQ 0x5411
#define TIOCSTI 0x5412
#define TIOCGWINSZ 0x5413
#define TIOCSWINSZ 0x5414
#define FIONREAD 0x541b
#define TCSBRKP 0x5425

#define TCIFLUSH 0
#define TCOFLUSH 1
#define TCIOFLUSH 2
#define TCOOFF 0
#define TCOON 1
#define TCIOFF 2
#define TCION 3

struct winsize {
  unsigned short ws_row; /* Character rows */
  unsigned short ws_col; /* Character columns */
  unsigned short ws_xpixel; /* Pixel width, if known */
  unsigned short ws_ypixel; /* Pixel height, if known */
};

/*
 * ioctl is the generic device-control boundary: the request determines whether
 * its optional argument is an integer or pointer. tc* functions provide typed
 * wrappers for common terminal requests and return -1 with errno on failure.
 */
int ioctl(int fd, unsigned long request, ...);
int tcgetattr(int fd, struct termios *termios_p);
int tcsetattr(int fd, int optional_actions, const struct termios *termios_p);
/* Drain output, control data flow, discard queued input/output, or send a break. */
int tcdrain(int fd);
int tcflow(int fd, int action);
int tcflush(int fd, int queue_selector);
int tcsendbreak(int fd, int duration);
int tcgetpgrp(int fd);
int tcsetpgrp(int fd, pid_t pgrp);
/* Return nonzero when fd refers to a terminal; otherwise return zero. */
int isatty(int fd);

/* Access and update baud-rate fields while preserving unrelated c_cflag bits. */
speed_t cfgetispeed(const struct termios *termios_p);
speed_t cfgetospeed(const struct termios *termios_p);
int cfsetispeed(struct termios *termios_p, speed_t speed);
int cfsetospeed(struct termios *termios_p, speed_t speed);
void cfmakeraw(struct termios *termios_p);

#endif
