#ifndef POLL_H
#define POLL_H

#include "./sys/types.h"

/* Readable conditions requested in pollfd.events. */
#define POLLIN 0x0001     /* Data can be read without blocking */
#define POLLPRI 0x0002    /* Urgent/priority data is available */
#define POLLOUT 0x0004    /* Writing can proceed without blocking */
#define POLLRDNORM 0x0040 /* Normal data is readable */
#define POLLRDBAND 0x0080 /* Priority-band data is readable */
#define POLLWRNORM 0x0100 /* Normal data can be written */
#define POLLWRBAND 0x0200 /* Priority-band data can be written */

/* Conditions reported in revents, even when not requested in events. */
#define POLLERR 0x0008    /* An error condition exists */
#define POLLHUP 0x0010    /* The other end hung up */
#define POLLNVAL 0x0020   /* fd is not an open descriptor */
#define POLLRDHUP 0x2000  /* Stream peer closed or shut down its write side */

typedef unsigned long nfds_t;

/*
 * One descriptor's interest mask and kernel-reported result mask. The short
 * field widths and ordering match Linux's x86_64 pollfd syscall ABI.
 */
struct pollfd {
  int fd;       /* Descriptor to examine; a negative value is ignored */
  short events; /* Requested readiness bits, such as POLLIN or POLLOUT */
  short revents; /* Actual conditions; cleared/replaced by poll on return */
};

/*
 * Wait for readiness across nfds descriptors. timeout is milliseconds:
 * negative waits indefinitely, zero checks without waiting, positive bounds
 * the wait. Returns the number of entries with nonzero revents, zero on
 * timeout, or -1 with errno set (for example EINTR if a signal interrupts it).
 */
int poll(struct pollfd *descriptors, nfds_t count, int timeout);

#endif
