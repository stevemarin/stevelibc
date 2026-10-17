#ifndef SYS_WAIT_H
#define SYS_WAIT_H

#include "./types.h"

/* waitpid options: return immediately, report stopped children, or report continuation. */
#define WNOHANG 1
#define WUNTRACED 2
#define WCONTINUED 8

/*
 * Linux packs a child's state into an int. These macros decode normal exits,
 * signal termination, stops, and continuation; the raw bit layout is a kernel
 * convention, so callers should test with the macros rather than bit-shifting.
 */
#define WIFEXITED(status) (((status) & 0x7f) == 0)
#define WEXITSTATUS(status) (((status) >> 8) & 0xff)
#define WIFSIGNALED(status) ((((status) & 0x7f) != 0) && (((status) & 0x7f) != 0x7f))
#define WTERMSIG(status) ((status) & 0x7f)
#define WIFSTOPPED(status) (((status) & 0xff) == 0x7f)
#define WSTOPSIG(status) (((status) >> 8) & 0xff)
#define WIFCONTINUED(status) ((status) == 0xffff)

pid_t wait(int *status);
/* pid selects a child/process group; status may be null and options combine W* flags. */
pid_t waitpid(pid_t pid, int *status, int options);

#endif
