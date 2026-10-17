#ifndef SYS_STAT_H
#define SYS_STAT_H

#include "./types.h"

#define S_IFMT 0170000
#define S_IFSOCK 0140000
#define S_IFLNK 0120000
#define S_IFREG 0100000
#define S_IFBLK 0060000
#define S_IFDIR 0040000
#define S_IFCHR 0020000
#define S_IFIFO 0010000

#define S_ISREG(mode) (((mode) & S_IFMT) == S_IFREG)
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#define S_ISCHR(mode) (((mode) & S_IFMT) == S_IFCHR)
#define S_ISBLK(mode) (((mode) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(mode) (((mode) & S_IFMT) == S_IFIFO)
#define S_ISLNK(mode) (((mode) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(mode) (((mode) & S_IFMT) == S_IFSOCK)

#define S_IRUSR 00400
#define S_IWUSR 00200
#define S_IXUSR 00100
#define S_IRGRP 00040
#define S_IWGRP 00020
#define S_IXGRP 00010
#define S_IROTH 00004
#define S_IWOTH 00002
#define S_IXOTH 00001

/*
 * Linux x86_64 kernel stat layout, not a portable POSIX layout. The kernel
 * writes this structure directly for fstat/newfstatat, so field order, widths,
 * and padding are ABI requirements. Keep the layout test in sync.
 */
struct stat {
  dev_t st_dev;   /* Device containing the file */
  ino_t st_ino;   /* Filesystem inode identifying this object */
  nlink_t st_nlink; /* Number of hard links */
  mode_t st_mode; /* File type and permission bits */
  uid_t st_uid;   /* Owner user ID */
  gid_t st_gid;   /* Owner group ID */
  int __pad0;     /* ABI padding; preserve for kernel layout */
  dev_t st_rdev;  /* Device ID when this is a device node */
  off_t st_size;  /* File length in bytes */
  long st_blksize; /* Preferred I/O block size */
  long st_blocks;  /* Allocated 512-byte blocks */
  struct { time_t tv_sec; long tv_nsec; } st_atim; /* Last data access */
  struct { time_t tv_sec; long tv_nsec; } st_mtim; /* Last data modification */
  struct { time_t tv_sec; long tv_nsec; } st_ctim; /* Last inode/status change */
  long __reserved[3]; /* Reserved ABI space; do not repurpose */
};

#define st_atime st_atim.tv_sec
#define st_mtime st_mtim.tv_sec
#define st_ctime st_ctim.tv_sec

/* Query metadata by path; stat follows a final symbolic link, lstat does not. */
int stat(const char *path, struct stat *buffer);
int lstat(const char *path, struct stat *buffer);
/* Descriptor and directory-relative forms avoid repeating path resolution. */
int fstat(int fd, struct stat *buffer);
int fstatat(int dirfd, const char *path, struct stat *buffer, int flags);

#endif
