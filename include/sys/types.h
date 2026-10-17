#ifndef SYS_TYPES_H
#define SYS_TYPES_H

/*
 * Project-local x86_64 Linux ABI types (the LP64 data model). File sizes,
 * offsets, and pointer-sized values use 64-bit long; process/user/group IDs
 * and mode_t use 32-bit integers. These minimal aliases can conflict with
 * host-libc headers that define the same names.
 */
/* Byte counts: size_t is unsigned; ssize_t can represent either a count or -1. */
typedef unsigned long size_t;
typedef long ptrdiff_t;
typedef long ssize_t;
/* Kernel process and credential identifiers. */
typedef int pid_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
/* Byte offsets and permission/type bits used by filesystem syscalls. */
typedef long off_t;
typedef unsigned int mode_t;
/* Filesystem device, inode, and link-count metadata. */
typedef unsigned long dev_t;
typedef unsigned long ino_t;
typedef unsigned long nlink_t;
/* Time and allocation measurements returned in filesystem metadata. */
typedef long time_t;
typedef long blksize_t;
typedef long blkcnt_t;
typedef long suseconds_t;

#endif
