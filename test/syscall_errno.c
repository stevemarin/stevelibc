#include "../include/errno.h"
#include "../include/unistd.h"

/*
 * Linux syscalls return negative errno values internally. Public wrappers
 * should convert those to -1 and store the positive error number in errno.
 */
int main(void) {
  char byte;

  errno = 0;
  if (read(-1, &byte, 1) != -1 || errno != EBADF)
    return 1;

  errno = 0;
  if (write(-1, "x", 1) != -1 || errno != EBADF)
    return 2;

  errno = 0;
  if (close(-1) != -1 || errno != EBADF)
    return 3;

  /* A successful call must not clear an errno left by an earlier failure. */
  errno = EINVAL;
  if (write(1, "", 0) != 0 || errno != EINVAL)
    return 4;

  /* execve must expose a missing executable as ENOENT. */
  errno = 0;
  if (execve("/stevelibc-test-file-that-does-not-exist", 0, 0) != -1 || errno != ENOENT)
    return 5;

  return 0;
}
