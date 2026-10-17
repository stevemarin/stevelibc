#include "../include/sys/stat.h"
#include "../include/fcntl.h"
#include "../include/syscall.h"

/*
 * Linux implements path-based stat calls with newfstatat. This shared entry
 * point lets stat/lstat differ only by the symlink-following flag.
 */
int fstatat(int dirfd, const char *path, struct stat *buffer, int flags) {
  return (int)__syscall_result(syscall4(SYS_newfstatat, dirfd, (long)path, (long)buffer, flags));
}

int stat(const char *path, struct stat *buffer) { return fstatat(AT_FDCWD, path, buffer, 0); }

int lstat(const char *path, struct stat *buffer) { return fstatat(AT_FDCWD, path, buffer, AT_SYMLINK_NOFOLLOW); }

int fstat(int fd, struct stat *buffer) {
  /* fstat has its own syscall, but writes the same kernel struct stat layout. */
  return (int)__syscall_result(syscall2(SYS_fstat, fd, (long)buffer));
}
