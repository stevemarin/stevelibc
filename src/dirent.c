#include "../include/dirent.h"
#include "../include/errno.h"
#include "../include/fcntl.h"
#include "../include/stddef.h"
#include "../include/stdlib.h"
#include "../include/syscall.h"
#include "../include/unistd.h"

#define DIRENT_BUFFER_SIZE 4096

typedef struct {
  /*
   * Linux getdents64 record: the kernel returns a sequence of these packed
   * headers followed by NUL-terminated names. Do not expose or trust this raw
   * layout until record_length and name bounds have been checked.
   */
  unsigned long long ino; /* Inode number */
  long long offset; /* Opaque continuation cookie */
  unsigned short record_length; /* Total record size, including header and name */
  unsigned char type; /* DT_* classification hint */
  char name[]; /* Variable-length NUL-terminated entry name */
} linux_dirent64;

struct __directory {
  int fd; /* Open directory descriptor owned by this handle */
  int position; /* Byte offset of the next raw record in buffer */
  int length; /* Number of valid bytes returned by getdents64 */
  char buffer[DIRENT_BUFFER_SIZE] __attribute__((aligned(8))); /* Batch of raw kernel records */
  struct dirent entry; /* Public copy returned by the most recent readdir */
};

DIR *opendir(const char *name) {
  /* O_DIRECTORY rejects non-directories; CLOEXEC avoids leaking across exec. */
  int fd = open(name, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (fd < 0)
    return 0;

  DIR *directory = (DIR *)malloc(sizeof(*directory));
  if (!directory) {
    int saved_errno = ENOMEM;
    close(fd);
    errno = saved_errno;
    return 0;
  }
  directory->fd = fd;
  directory->position = 0;
  directory->length = 0;
  return directory;
}

struct dirent *readdir(DIR *directory) {
  if (!directory) {
    errno = EBADF;
    return 0;
  }

  for (;;) {
    if (directory->position >= directory->length) {
      /* EOF is a zero-byte getdents64 result; errors are converted to errno. */
      long bytes =
          __syscall_result(syscall3(SYS_getdents64, directory->fd, (long)directory->buffer, DIRENT_BUFFER_SIZE));
      if (bytes <= 0)
        return 0;
      directory->position = 0;
      directory->length = (int)bytes;
    }

    if (directory->length - directory->position < (int)offsetof(linux_dirent64, name)) {
      errno = EIO;
      return 0;
    }
    /*
     * Validate every variable-length record before reading its name or moving
     * to the next record; the kernel buffer is an untrusted byte stream here.
     */
    linux_dirent64 *raw = (linux_dirent64 *)(directory->buffer + directory->position);
    if (raw->record_length < offsetof(linux_dirent64, name) + 1 ||
        raw->record_length > directory->length - directory->position) {
      errno = EIO;
      return 0;
    }

    int name_capacity = raw->record_length - (int)offsetof(linux_dirent64, name);
    int name_length = 0;
    while (name_length < name_capacity && raw->name[name_length])
      name_length++;
    if (name_length == name_capacity || name_length >= (int)sizeof(directory->entry.d_name)) {
      errno = EIO;
      return 0;
    }

    /* Copy into stable public storage; the next readdir call overwrites it. */
    /*
     * dirent storage belongs to DIR, not the syscall buffer; callers may read
     * the result until their next readdir call, when this one entry is reused.
     */
    directory->entry.d_ino = (ino_t)raw->ino;
    directory->entry.d_off = (long)raw->offset;
    directory->entry.d_reclen = (unsigned short)raw->record_length;
    directory->entry.d_type = raw->type;
    for (int i = 0; i <= name_length; i++)
      directory->entry.d_name[i] = raw->name[i];
    directory->position += raw->record_length;
    return &directory->entry;
  }
}

int closedir(DIR *directory) {
  if (!directory) {
    errno = EBADF;
    return -1;
  }
  int result = close(directory->fd);
  int saved_errno = errno;
  free(directory);
  if (result < 0)
    errno = saved_errno;
  return result;
}

int dirfd(DIR *directory) {
  if (!directory) {
    errno = EBADF;
    return -1;
  }
  return directory->fd;
}
