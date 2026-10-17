#include "../include/poll.h"
#include "../include/syscall.h"

/*
 * Linux poll(2) consumes an array of pollfd records directly. Keeping this
 * wrapper thin lets the kernel handle races between checking descriptors and
 * waiting for their readiness, which user-space probing cannot do safely.
 */
int poll(struct pollfd *descriptors, nfds_t count, int timeout) {
  return (int)__syscall_result(syscall3(SYS_poll, (long)descriptors, count, timeout));
}
