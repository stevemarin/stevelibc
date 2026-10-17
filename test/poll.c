#include "../include/errno.h"
#include "../include/poll.h"
#include "../include/stddef.h"
#include "../include/unistd.h"

int main(void) {
  /* The public record must match the kernel's int/short/short ABI exactly. */
  if (sizeof(struct pollfd) != 8 || offsetof(struct pollfd, events) != 4 ||
      offsetof(struct pollfd, revents) != 6)
    return 13;

  int descriptors[2];
  if (pipe(descriptors) != 0)
    return 1;

  /* An empty pipe has no readable bytes; a zero timeout only probes state. */
  struct pollfd watched = {.fd = descriptors[0], .events = POLLIN, .revents = -1};
  if (poll(&watched, 1, 0) != 0 || watched.revents != 0)
    return 2;

  /* A pipe's write end is ready while space remains, independently of input. */
  struct pollfd writable = {.fd = descriptors[1], .events = POLLOUT, .revents = 0};
  if (poll(&writable, 1, 0) != 1 || !(writable.revents & POLLOUT))
    return 3;

  /* The reader becomes ready after a byte arrives; poll returns ready entries. */
  if (write(descriptors[1], "r", 1) != 1)
    return 4;
  if (poll(&watched, 1, 0) != 1 || !(watched.revents & POLLIN))
    return 5;
  char byte;
  if (read(descriptors[0], &byte, 1) != 1 || byte != 'r')
    return 6;

  /* Negative descriptors are deliberately ignored and report no events. */
  struct pollfd ignored = {.fd = -1, .events = POLLIN, .revents = -1};
  if (poll(&ignored, 1, 0) != 0 || ignored.revents != 0)
    return 7;

  /* A closed descriptor reports POLLNVAL; closing the writer reports POLLHUP. */
  if (close(descriptors[1]) != 0)
    return 8;
  writable.revents = 0;
  if (poll(&writable, 1, 0) != 1 || !(writable.revents & POLLNVAL))
    return 9;
  watched.revents = 0;
  if (poll(&watched, 1, 0) != 1 || !(watched.revents & POLLHUP))
    return 10;
  if (close(descriptors[0]) != 0)
    return 14;

  /* Zero descriptors still allow poll to serve as a millisecond sleep. */
  if (poll(0, 0, 1) != 0)
    return 11;

  /* Invalid pointers and counts above the process descriptor limit are errors. */
  errno = 0;
  if (poll(0, 1, 0) != -1 || errno != EFAULT)
    return 12;
  errno = 0;
  if (poll(&watched, ~0UL, 0) != -1 || errno != EINVAL)
    return 15;

  return 0;
}
