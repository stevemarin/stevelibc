#ifndef DIRENT_H
#define DIRENT_H

#include "./sys/types.h"

#define DT_UNKNOWN 0
#define DT_FIFO 1
#define DT_CHR 2
#define DT_DIR 4
#define DT_BLK 6
#define DT_REG 8
#define DT_LNK 10
#define DT_SOCK 12

/*
 * Stable, libc-facing view of one directory entry. readdir copies the
 * variable-length Linux getdents64 record into this structure, which belongs
 * to the DIR handle and is overwritten by the next readdir on that handle.
 */
struct dirent {
  ino_t d_ino;              /* Inode number within the containing filesystem */
  long d_off;               /* Opaque position cookie for directory iteration */
  unsigned short d_reclen;  /* Length of the original kernel record */
  unsigned char d_type;     /* DT_* hint; DT_UNKNOWN is permitted */
  char d_name[256];         /* NUL-terminated name copied from the kernel record */
};

/* Opaque iterator state: callers use the handle only through this API. */
typedef struct __directory DIR;

/* Open a directory stream backed by a close-on-exec file descriptor. */
DIR *opendir(const char *name);
/* Return the next entry, or NULL at end of stream or on error (errno is set). */
struct dirent *readdir(DIR *directory);
/* Close the stream's descriptor and release its user-space iterator state. */
int closedir(DIR *directory);
/* Expose the descriptor underlying an open directory stream. */
int dirfd(DIR *directory);

#endif
