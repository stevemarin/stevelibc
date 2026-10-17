#ifndef FCNTL_H
#define FCNTL_H

#include "./sys/types.h"

/*
 * Linux x86_64 open(2)/openat(2) flags. Access mode occupies the low two bits;
 * the remaining flags modify creation, truncation, append, or descriptor behavior.
 */
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_CREAT 0100
#define O_EXCL 0200
#define O_NOCTTY 0400
#define O_TRUNC 01000
#define O_APPEND 02000
#define O_NONBLOCK 04000
#define O_DIRECTORY 0200000
#define O_CLOEXEC 02000000

/* fcntl command numbers and descriptor flag bits passed directly to the kernel. */
#define F_DUPFD 0
#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 4
#define FD_CLOEXEC 1

/*
 * Special directory descriptor and path-resolution flags for *at operations.
 * AT_FDCWD means resolve a relative path against the process current directory.
 */
#define AT_FDCWD (-100)
#define AT_SYMLINK_NOFOLLOW 0x100
#define AT_REMOVEDIR 0x200
#define AT_SYMLINK_FOLLOW 0x400

/* The variadic mode argument is required when O_CREAT is present. */
int open(const char *path, int flags, ...);
int openat(int dirfd, const char *path, int flags, ...);
int fcntl(int fd, int command, ...);

#endif
