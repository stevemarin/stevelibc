#include "../include/stdlib.h"
#include "../include/syscall.h"
#include "../include/unistd.h"

/* ==========================================================================
 * STEVELIBC.C : Raw Linux system-call wrappers for common process and file APIs
 * ========================================================================== */

/* Return byte counts on success; convert kernel -errno values to libc errors. */
ssize_t read(int fd, void *buf, size_t count) { return __syscall_result(syscall3(SYS_read, fd, (long)buf, count)); }

ssize_t write(int fd, const void *buf, size_t count) {
  return __syscall_result(syscall3(SYS_write, fd, (long)buf, count));
}

int open(const char *path, int flags, ...) {
  unsigned int mode = 0;
  if (flags & O_CREAT) {
    /* The mode argument is present only when a new file may be created. */
    __builtin_va_list args;
    __builtin_va_start(args, flags);
    mode = __builtin_va_arg(args, unsigned int);
    __builtin_va_end(args);
  }
  return (int)__syscall_result(syscall3(SYS_open, (long)path, flags, mode));
}

int openat(int dirfd, const char *path, int flags, ...) {
  unsigned int mode = 0;
  if (flags & O_CREAT) {
    __builtin_va_list args;
    __builtin_va_start(args, flags);
    mode = __builtin_va_arg(args, unsigned int);
    __builtin_va_end(args);
  }
  return (int)__syscall_result(syscall4(SYS_openat, dirfd, (long)path, flags, mode));
}

int close(int fd) { return (int)__syscall_result(syscall1(SYS_close, fd)); }

off_t lseek(int fd, off_t offset, int whence) { return __syscall_result(syscall3(SYS_lseek, fd, offset, whence)); }

int dup(int fd) { return (int)__syscall_result(syscall1(SYS_dup, fd)); }

int dup2(int oldfd, int newfd) { return (int)__syscall_result(syscall2(SYS_dup2, oldfd, newfd)); }

int pipe(int pipefd[2]) {
  if (!pipefd) {
    /* Avoid passing a null output array to the kernel and report the fault. */
    errno = EFAULT;
    return -1;
  }
  return (int)__syscall_result(syscall1(SYS_pipe, (long)pipefd));
}

int access(const char *path, int mode) { return (int)__syscall_result(syscall2(SYS_access, (long)path, mode)); }

int faccessat(int dirfd, const char *path, int mode) {
  return (int)__syscall_result(syscall3(SYS_faccessat, dirfd, (long)path, mode));
}

int unlink(const char *path) { return (int)__syscall_result(syscall1(SYS_unlink, (long)path)); }

int unlinkat(int dirfd, const char *path, int flags) {
  return (int)__syscall_result(syscall3(SYS_unlinkat, dirfd, (long)path, flags));
}

int mkdir(const char *path, unsigned int mode) { return (int)__syscall_result(syscall2(SYS_mkdir, (long)path, mode)); }

int mkdirat(int dirfd, const char *path, mode_t mode) {
  return (int)__syscall_result(syscall3(SYS_mkdirat, dirfd, (long)path, mode));
}

int rmdir(const char *path) { return (int)__syscall_result(syscall1(SYS_rmdir, (long)path)); }

/*
 * Directory-relative calls let a program resolve several names beneath one
 * already-open directory. The kernel applies the operation atomically, avoiding
 * the races inherent in constructing absolute paths in user space.
 */
int rename(const char *oldpath, const char *newpath) {
  return (int)__syscall_result(syscall2(SYS_rename, (long)oldpath, (long)newpath));
}

int renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath) {
  return (int)__syscall_result(syscall4(SYS_renameat, olddirfd, (long)oldpath, newdirfd, (long)newpath));
}

int link(const char *oldpath, const char *newpath) {
  return (int)__syscall_result(syscall2(SYS_link, (long)oldpath, (long)newpath));
}

int linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags) {
  return (int)__syscall_result(syscall5(SYS_linkat, olddirfd, (long)oldpath, newdirfd, (long)newpath, flags));
}

int symlink(const char *target, const char *linkpath) {
  return (int)__syscall_result(syscall2(SYS_symlink, (long)target, (long)linkpath));
}

int symlinkat(const char *target, int newdirfd, const char *linkpath) {
  return (int)__syscall_result(syscall3(SYS_symlinkat, (long)target, newdirfd, (long)linkpath));
}

ssize_t readlink(const char *path, char *buffer, size_t size) {
  /* readlink returns bytes copied and deliberately does not append '\0'. */
  return __syscall_result(syscall3(SYS_readlink, (long)path, (long)buffer, size));
}

ssize_t readlinkat(int dirfd, const char *path, char *buffer, size_t size) {
  return __syscall_result(syscall4(SYS_readlinkat, dirfd, (long)path, (long)buffer, size));
}

int chmod(const char *path, mode_t mode) { return (int)__syscall_result(syscall2(SYS_chmod, (long)path, mode)); }

int fchmod(int fd, mode_t mode) { return (int)__syscall_result(syscall2(SYS_fchmod, fd, mode)); }

int ftruncate(int fd, off_t length) { return (int)__syscall_result(syscall2(SYS_ftruncate, fd, length)); }

int truncate(const char *path, off_t length) {
  return (int)__syscall_result(syscall2(SYS_truncate, (long)path, length));
}

int fsync(int fd) { return (int)__syscall_result(syscall1(SYS_fsync, fd)); }

/* umask returns the previous mask directly; no negative errno is possible. */
mode_t umask(mode_t mask) { return (mode_t)syscall1(SYS_umask, mask); }

int chdir(const char *path) { return (int)__syscall_result(syscall1(SYS_chdir, (long)path)); }

int fchdir(int fd) { return (int)__syscall_result(syscall1(SYS_fchdir, fd)); }

int fcntl(int fd, int command, ...) {
  long argument = 0;
  /* Only these commands consume the optional integer argument in this subset. */
  if (command == F_DUPFD || command == F_SETFD || command == F_SETFL) {
    __builtin_va_list args;
    __builtin_va_start(args, command);
    argument = __builtin_va_arg(args, int);
    __builtin_va_end(args);
  }
  return (int)__syscall_result(syscall3(SYS_fcntl, fd, command, argument));
}

char *getcwd(char *buffer, size_t size) {
  /* Linux returns the byte count on success; POSIX returns the supplied buffer. */
  long result = __syscall_result(syscall2(SYS_getcwd, (long)buffer, (long)size));
  return result < 0 ? 0 : buffer;
}

pid_t waitpid(pid_t pid, int *status, int options) {
  /* wait4 is Linux's underlying operation; a null rusage pointer is sufficient. */
  return (pid_t)__syscall_result(syscall4(SYS_wait4, pid, (long)status, options, 0));
}

pid_t wait(int *status) { return waitpid(-1, status, 0); }

int kill(pid_t pid, int signal) { return (int)__syscall_result(syscall2(SYS_kill, pid, signal)); }

int setpgid(pid_t pid, pid_t pgid) { return (int)__syscall_result(syscall2(SYS_setpgid, pid, pgid)); }

pid_t getpgid(pid_t pid) { return (pid_t)__syscall_result(syscall1(SYS_getpgid, pid)); }

pid_t setsid(void) { return (pid_t)__syscall_result(syscall0(SYS_setsid)); }

pid_t getsid(pid_t pid) { return (pid_t)__syscall_result(syscall1(SYS_getsid, pid)); }

/* These identity syscalls cannot fail for a running process. */
pid_t getpid(void) { return (pid_t)syscall0(SYS_getpid); }

pid_t getppid(void) { return (pid_t)syscall0(SYS_getppid); }

uid_t getuid(void) { return (uid_t)syscall0(SYS_getuid); }

uid_t geteuid(void) { return (uid_t)syscall0(SYS_geteuid); }

gid_t getgid(void) { return (gid_t)syscall0(SYS_getgid); }

gid_t getegid(void) { return (gid_t)syscall0(SYS_getegid); }

int fork(void) { return (int)__syscall_result(syscall0(SYS_fork)); }

/* On success execve does not return; a return therefore always indicates error. */
int execve(const char *filename, char *const argv[], char *const envp[]) {
  return (int)__syscall_result(syscall3(SYS_execve, (long)filename, (long)argv, (long)envp));
}

int execv(const char *filename, char *const argv[]) { return execve(filename, argv, environ); }

/*
 * Search PATH from left to right. An empty component denotes the current
 * directory. Preserve EACCES if at least one candidate existed but could not
 * be executed; otherwise report ENOENT after every candidate is exhausted.
 *
 * This compact teaching implementation deliberately omits execvp's optional
 * /bin/sh fallback for a text file rejected with ENOEXEC.
 */
int execvp(const char *filename, char *const argv[]) {
  if (!filename || !*filename) {
    errno = EINVAL;
    return -1;
  }
  for (const char *p = filename; *p; p++) {
    if (*p == '/')
      return execv(filename, argv);
  }

  const char *search_path = getenv("PATH");
  if (!search_path)
    search_path = "/bin:/usr/bin";

  int saw_access_error = 0;
  const char *component = search_path;
  for (;;) {
    const char *end = component;
    while (*end && *end != ':')
      end++;
    size_t directory_length = (size_t)(end - component);
    size_t filename_length = 0;
    while (filename[filename_length])
      filename_length++;
    size_t separator_length = directory_length ? 1 : 0;
    if (filename_length > ~0UL - separator_length - 1) {
      errno = ENAMETOOLONG;
      return -1;
    }
    size_t suffix_length = filename_length + separator_length + 1;
    if (directory_length > ~0UL - suffix_length) {
      errno = ENAMETOOLONG;
      return -1;
    }

    char *candidate = (char *)malloc(directory_length + suffix_length);
    if (!candidate)
      return -1;
    size_t at = 0;
    for (size_t i = 0; i < directory_length; i++)
      candidate[at++] = component[i];
    if (separator_length)
      candidate[at++] = '/';
    for (size_t i = 0; i < filename_length; i++)
      candidate[at++] = filename[i];
    candidate[at] = '\0';

    (void)execve(candidate, argv, environ);
    int candidate_errno = errno;
    free(candidate);
    if (candidate_errno == EACCES) {
      saw_access_error = 1;
    } else if (candidate_errno != ENOENT && candidate_errno != ENOTDIR) {
      errno = candidate_errno;
      return -1;
    }

    if (!*end)
      break;
    component = end + 1;
  }
  errno = saw_access_error ? EACCES : ENOENT;
  return -1;
}

void _exit(int status) {
  /* Unlike exit(), this bypasses stream flushing and registered handlers. */
  syscall1(SYS_exit_group, status);
  __builtin_unreachable();
}
