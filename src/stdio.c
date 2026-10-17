#include "../include/stdio.h"
#include "../include/errno.h"
#include "../include/stddef.h"
#include "../include/stdlib.h"
#include "../include/string.h"
#include "../include/syscall.h"
#include "../include/unistd.h"

/* ==========================================================================
 * STDIO.C : Small buffered I/O implementation
 * --------------------------------------------------------------------------
 * This file implements the stdio subset declared in stdio.h. Streams use a
 * single buffer for input or output at a time. The low bits of FILE.flags
 * describe access and ownership; the higher bits track status and direction.
 * ========================================================================== */

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_CREAT 0100
#define O_TRUNC 01000
#define O_APPEND 02000

#define _IOREAD 1
#define _IOWRITE 2
#define _IOOWNED 4
#define _IOERROR 16
#define _IOEOF 32
#define _IOBUFOWNED 64
#define _IOREADING 128
#define _IOWRITING 256

#define INT_MAX_VALUE 2147483647
#define LONG_MAX_VALUE ((long)(~0UL >> 1))
#define LONG_MIN_VALUE (-LONG_MAX_VALUE - 1L)

/* Static storage keeps standard streams available without allocating at boot. */
static char stdin_buf[BUFSIZ];
static char stdout_buf[BUFSIZ];
static char stderr_buf[BUFSIZ];

static FILE __stdin = {
    .fd = 0,
    .buf = stdin_buf,
    .buf_size = BUFSIZ,
    .mode = _IOLBF,
    .flags = _IOREAD,
};
static FILE __stdout = {
    .fd = 1,
    .buf = stdout_buf,
    .buf_size = BUFSIZ,
    .mode = _IOLBF,
    .flags = _IOWRITE,
};
static FILE __stderr = {
    .fd = 2,
    .buf = stderr_buf,
    .buf_size = BUFSIZ,
    .mode = _IONBF,
    .flags = _IOWRITE,
};

FILE *stdin = &__stdin;
FILE *stdout = &__stdout;
FILE *stderr = &__stderr;

/* Dynamically opened streams are linked here so fflush(NULL) can visit them. */
static FILE *open_streams;

/*
 * FILE locks are recursive because formatted I/O calls fputc repeatedly and
 * bulk operations call the character primitives. Linux TIDs distinguish
 * owners even though errno's TLS storage is project-specific.
 */
void flockfile(FILE *stream) {
  if (!stream)
    return;
  int tid = (int)syscall0(SYS_gettid);
  for (;;) {
    int owner = __atomic_load_n(&stream->lock_owner, __ATOMIC_ACQUIRE);
    if (owner == tid) {
      stream->lock_depth++;
      return;
    }
    int unlocked = 0;
    /*
     * Claim ownership atomically because another thread may be acquiring the
     * same stream at the same time. Acquire/release ordering publishes buffer
     * updates between successive owners.
     */
    if (__atomic_compare_exchange_n(&stream->lock_owner, &unlocked, tid, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
      stream->lock_depth = 1;
      return;
    }
    /* This deliberately simple lock spins rather than depending on futexes. */
    __asm__ volatile("pause");
  }
}

int ftrylockfile(FILE *stream) {
  if (!stream)
    return -1;
  int tid = (int)syscall0(SYS_gettid);
  int owner = __atomic_load_n(&stream->lock_owner, __ATOMIC_ACQUIRE);
  if (owner == tid) {
    stream->lock_depth++;
    return 0;
  }
  int unlocked = 0;
  if (!__atomic_compare_exchange_n(&stream->lock_owner, &unlocked, tid, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
    return -1;
  stream->lock_depth = 1;
  return 0;
}

void funlockfile(FILE *stream) {
  if (!stream)
    return;
  int tid = (int)syscall0(SYS_gettid);
  if (__atomic_load_n(&stream->lock_owner, __ATOMIC_RELAXED) != tid || stream->lock_depth <= 0)
    return;
  stream->lock_depth--;
  if (stream->lock_depth == 0)
    __atomic_store_n(&stream->lock_owner, 0, __ATOMIC_RELEASE);
}

static int stream_valid(FILE *stream) {
  if (!stream || stream->fd < 0) {
    errno = EBADF;
    if (stream)
      stream->flags |= _IOERROR;
    return 0;
  }
  return 1;
}

static int stream_can_read(FILE *stream) {
  if (!stream_valid(stream))
    return 0;
  if (!(stream->flags & _IOREAD)) {
    errno = EBADF;
    stream->flags |= _IOERROR;
    return 0;
  }
  return 1;
}

static int stream_can_write(FILE *stream) {
  if (!stream_valid(stream))
    return 0;
  if (!(stream->flags & _IOWRITE)) {
    errno = EBADF;
    stream->flags |= _IOERROR;
    return 0;
  }
  return 1;
}

/*
 * Bring an input stream's kernel offset back to the next unread byte.
 * read() may have filled the buffer farther ahead than the application read;
 * include a pushed-back byte because it is also logically unread.
 */
static int discard_input(FILE *stream) {
  /*
   * The descriptor offset follows the whole read(2) refill, not the last byte
   * delivered to the caller. Rewind the unread bytes before changing direction
   * so a following write or seek uses the application's logical position.
   */
  int unread = stream->buf_len - stream->buf_pos + (stream->has_unget != 0);
  if (unread > 0) {
    long result = __syscall_result(syscall3(SYS_lseek, stream->fd, -unread, SEEK_CUR));
    if (result < 0) {
      stream->flags |= _IOERROR;
      return -1;
    }
  }

  stream->buf_pos = 0;
  stream->buf_len = 0;
  stream->has_unget = 0;
  stream->flags &= ~_IOREADING;
  return 0;
}

/*
 * Flush buffered output completely. write(2) is allowed to return short
 * counts; on failure, keep the unwritten suffix in the stream buffer.
 */
static int flush_output(FILE *stream) {
  int written = 0;
  while (written < stream->buf_pos) {
    long result = __syscall_result(
        syscall3(SYS_write, stream->fd, (long)(stream->buf + written), (unsigned long)(stream->buf_pos - written)));
    if (result < 0 || result == 0) {
      if (result == 0)
        errno = EIO;

      /*
       * Keep the unwritten suffix at the front for a later retry. Discarding
       * it here would turn a temporary write failure into silent data loss.
       */
      for (int i = written; i < stream->buf_pos; i++)
        stream->buf[i - written] = stream->buf[i];
      stream->buf_pos -= written;
      stream->flags |= _IOERROR;
      return -1;
    }
    written += (int)result;
  }

  stream->buf_pos = 0;
  stream->flags &= ~_IOWRITING;
  return 0;
}

/* Flush or synchronize one stream; used by fflush(stream) and fflush(NULL). */
static int flush_one(FILE *stream) {
  flockfile(stream);
  if (!stream_valid(stream))
    goto flush_error;

  if (stream->flags & _IOWRITING) {
    int result = flush_output(stream);
    funlockfile(stream);
    return result;
  }

  /*
   * For seekable input files POSIX fflush synchronizes the descriptor offset.
   * On a pipe or terminal, seeking unread buffered data fails with ESPIPE.
   */
  if ((stream->flags & _IOREADING) && (stream->buf_pos < stream->buf_len || stream->has_unget)) {
    int result = discard_input(stream);
    funlockfile(stream);
    return result;
  }

  funlockfile(stream);
  return 0;

flush_error:
  funlockfile(stream);
  return -1;
}

/*
 * Flush the specified stream, or all standard and opened output streams when
 * stream is NULL. Continue visiting streams after a failure, but return EOF
 * if any flush failed.
 */
int fflush(FILE *stream) {
  if (stream)
    return flush_one(stream) == 0 ? 0 : EOF;

  int result = 0;
  if (flush_one(stdout) != 0)
    result = EOF;
  if (flush_one(stderr) != 0)
    result = EOF;
  for (FILE *current = open_streams; current; current = current->next) {
    /* fflush(NULL) flushes pending output, not buffered input streams. */
    if ((current->flags & (_IOWRITE | _IOWRITING)) == (_IOWRITE | _IOWRITING) && flush_output(current) != 0)
      result = EOF;
  }
  return result;
}

/*
 * Return the logical stream position rather than the kernel descriptor offset.
 * Input buffering advances the descriptor beyond unread bytes; pending output
 * has not advanced it yet. Pushback also moves the logical read position back.
 */
static long ftell_unlocked(FILE *stream) {
  if (!stream_valid(stream))
    return -1;

  long position = __syscall_result(syscall3(SYS_lseek, stream->fd, 0, SEEK_CUR));
  if (position < 0) {
    stream->flags |= _IOERROR;
    return -1;
  }

  if (stream->flags & _IOREADING) {
    long unread = stream->buf_len - stream->buf_pos + (stream->has_unget != 0);
    if (position < unread) {
      errno = EIO;
      stream->flags |= _IOERROR;
      return -1;
    }
    position -= unread;
  } else if (stream->flags & _IOWRITING) {
    if (position > LONG_MAX_VALUE - stream->buf_pos) {
      errno = EOVERFLOW;
      stream->flags |= _IOERROR;
      return -1;
    }
    position += stream->buf_pos;
  }
  return position;
}

/*
 * Move the kernel offset while preserving stdio's logical offset. For SEEK_CUR,
 * subtract bytes already read ahead so the caller's offset is relative to the
 * next byte the application will read, not the end of the input buffer.
 */
long ftell(FILE *stream) {
  flockfile(stream);
  long result = ftell_unlocked(stream);
  funlockfile(stream);
  return result;
}

static int fseek_unlocked(FILE *stream, long offset, int whence) {
  if (!stream_valid(stream))
    return -1;
  if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) {
    errno = EINVAL;
    stream->flags |= _IOERROR;
    return -1;
  }

  if ((stream->flags & _IOWRITING) && flush_output(stream) != 0)
    return -1;

  if (whence == SEEK_CUR && (stream->flags & _IOREADING)) {
    /* SEEK_CUR starts at the descriptor's read-ahead position; compensate. */
    long unread = stream->buf_len - stream->buf_pos + (stream->has_unget != 0);
    if (offset < LONG_MIN_VALUE + unread) {
      errno = EOVERFLOW;
      stream->flags |= _IOERROR;
      return -1;
    }
    offset -= unread;
  }

  long result = __syscall_result(syscall3(SYS_lseek, stream->fd, offset, whence));
  if (result < 0) {
    stream->flags |= _IOERROR;
    return -1;
  }

  stream->buf_pos = 0;
  stream->buf_len = 0;
  stream->has_unget = 0;
  stream->flags &= ~(_IOEOF | _IOREADING | _IOWRITING);
  return 0;
}

int fseek(FILE *stream, long offset, int whence) {
  flockfile(stream);
  int result = fseek_unlocked(stream, offset, whence);
  funlockfile(stream);
  return result;
}

void rewind(FILE *stream) {
  if (fseek(stream, 0, SEEK_SET) == 0)
    clearerr(stream);
}

/*
 * Change buffering before the stream has been used. A NULL buffer asks stdio
 * to allocate one; _IONBF does not need any buffer storage.
 */
static int setvbuf_unlocked(FILE *stream, char *buffer, int mode, unsigned long size) {
  if (!stream_valid(stream) || (mode != _IOFBF && mode != _IOLBF && mode != _IONBF) ||
      (mode != _IONBF && (size == 0 || size > INT_MAX_VALUE))) {
    errno = EINVAL;
    if (stream)
      stream->flags |= _IOERROR;
    return -1;
  }

  if (stream->buf_pos != 0 || stream->buf_len != 0 || stream->has_unget != 0 ||
      (stream->flags & (_IOREADING | _IOWRITING))) {
    errno = EBUSY;
    stream->flags |= _IOERROR;
    return -1;
  }

  char *new_buffer = mode == _IONBF ? 0 : buffer;
  int owns_buffer = 0;
  if (mode != _IONBF && !new_buffer) {
    /* A NULL buffer asks stdio to allocate and later release its storage. */
    new_buffer = (char *)malloc(size);
    if (!new_buffer) {
      errno = ENOMEM;
      stream->flags |= _IOERROR;
      return -1;
    }
    owns_buffer = 1;
  }

  /*
   * Pointer equality is not enough to determine ownership: caller buffers
   * must not be freed, while a stream's existing allocated buffer may remain.
   */
  if (mode != _IONBF && new_buffer == stream->buf && (stream->flags & _IOBUFOWNED))
    owns_buffer = 1;
  else if (stream->flags & _IOBUFOWNED)
    free(stream->buf);

  stream->buf = new_buffer;
  stream->buf_size = mode == _IONBF ? 0 : (int)size;
  stream->mode = mode;
  stream->flags &= ~_IOBUFOWNED;
  if (owns_buffer)
    stream->flags |= _IOBUFOWNED;
  return 0;
}

int setvbuf(FILE *stream, char *buffer, int mode, unsigned long size) {
  flockfile(stream);
  int result = setvbuf_unlocked(stream, buffer, mode, size);
  funlockfile(stream);
  return result;
}

void setbuf(FILE *stream, char *buffer) {
  if (buffer)
    (void)setvbuf(stream, buffer, _IOFBF, BUFSIZ);
  else
    (void)setvbuf(stream, 0, _IONBF, 0);
}

/* Write one byte, preserving the standard int return value of fputc. */
static int fputc_unlocked(int c, FILE *stream) {
  if (!stream_can_write(stream))
    return EOF;

  /*
   * snprintf uses the same formatter but a non-I/O sink: count every byte,
   * while storing only bytes that fit before the required terminator.
   */
  if (stream->string_mode) {
    if (stream->string_count < (unsigned long)stream->buf_size)
      stream->buf[stream->string_count] = (char)(unsigned char)c;
    stream->string_count++;
    return (unsigned char)c;
  }

  if (stream->flags & _IOREADING) {
    /*
     * Reconcile buffered input first. Otherwise the descriptor could be
     * positioned after read-ahead while the caller believes it is earlier.
     */
    if (discard_input(stream) != 0)
      return EOF;
  }

  unsigned char byte = (unsigned char)c;
  if (stream->mode == _IONBF) {
    long result = __syscall_result(syscall3(SYS_write, stream->fd, (long)&byte, 1));
    if (result != 1) {
      if (result == 0)
        errno = EIO;
      stream->flags |= _IOERROR;
      return EOF;
    }
    return byte;
  }

  if (!stream->buf || stream->buf_size <= 0) {
    errno = EINVAL;
    stream->flags |= _IOERROR;
    return EOF;
  }

  if (stream->buf_pos >= stream->buf_size && flush_output(stream) != 0)
    return EOF;

  stream->buf[stream->buf_pos++] = (char)byte;
  stream->flags |= _IOWRITING;

  if (stream->mode == _IOLBF && byte == '\n' && flush_output(stream) != 0)
    return EOF;

  return byte;
}

int fputc(int c, FILE *stream) {
  flockfile(stream);
  int result = fputc_unlocked(c, stream);
  funlockfile(stream);
  return result;
}

int putchar(int c) { return fputc(c, stdout); }

/* Read one byte, refilling the user-space buffer only when it is exhausted. */
static int fgetc_unlocked(FILE *stream) {
  if (!stream_can_read(stream))
    return EOF;

  if (stream->flags & _IOWRITING) {
    if (flush_output(stream) != 0)
      return EOF;
  }

  if (stream->has_unget) {
    stream->has_unget = 0;
    stream->flags |= _IOREADING;
    return (unsigned char)stream->unget_buf;
  }

  /* Unbuffered input reads directly into a local byte rather than a FILE buffer. */
  if (stream->mode == _IONBF) {
    unsigned char byte;
    long count;
    do {
      count = __syscall_result(syscall3(SYS_read, stream->fd, (long)&byte, 1));
    } while (count < 0 && errno == EINTR);
    if (count < 0) {
      stream->flags |= _IOERROR;
      return EOF;
    }
    if (count == 0) {
      stream->flags |= _IOEOF;
      return EOF;
    }
    stream->flags |= _IOREADING;
    return byte;
  }

  if (stream->buf_pos >= stream->buf_len) {
    if (!stream->buf || stream->buf_size <= 0) {
      errno = EINVAL;
      stream->flags |= _IOERROR;
      return EOF;
    }

    long count;
    do {
      count = __syscall_result(syscall3(SYS_read, stream->fd, (long)stream->buf, (unsigned long)stream->buf_size));
    } while (count < 0 && errno == EINTR);

    if (count < 0) {
      stream->flags |= _IOERROR;
      return EOF;
    }
    if (count == 0) {
      stream->flags |= _IOEOF;
      return EOF;
    }

    stream->buf_pos = 0;
    stream->buf_len = (int)count;
  }

  stream->flags |= _IOREADING;
  return (unsigned char)stream->buf[stream->buf_pos++];
}

int fgetc(FILE *stream) {
  flockfile(stream);
  int result = fgetc_unlocked(stream);
  funlockfile(stream);
  return result;
}

/*
 * Build one C string from a stream. The stream lock spans the whole line read
 * so another thread cannot interleave bytes between successive fgetc calls.
 */
char *fgets(char *buffer, int size, FILE *stream) {
  if (size <= 0 || !buffer) {
    errno = EINVAL;
    if (stream) {
      flockfile(stream);
      stream->flags |= _IOERROR;
      funlockfile(stream);
    }
    return 0;
  }
  if (!stream) {
    errno = EINVAL;
    return 0;
  }

  flockfile(stream);
  if (size == 1) {
    /* No data byte fits, but the caller still receives a valid empty string. */
    buffer[0] = '\0';
    funlockfile(stream);
    return buffer;
  }

  int used = 0;
  while (used < size - 1) {
    int byte = fgetc(stream);
    if (byte == EOF)
      break;
    buffer[used++] = (char)(unsigned char)byte;
    if (byte == '\n')
      break;
  }
  buffer[used] = '\0';

  /* A read error fails the operation; ordinary EOF may return a partial line. */
  int failed = ferror(stream);
  int ended = feof(stream);
  funlockfile(stream);
  if (failed || (used == 0 && ended))
    return 0;
  return buffer;
}

int getchar(void) { return fgetc(stdin); }

/* Push back one byte; the supported subset guarantees at least one byte. */
static int ungetc_unlocked(int c, FILE *stream) {
  if (c == EOF || !stream_can_read(stream))
    return EOF;
  if (stream->has_unget)
    return EOF;
  if ((stream->flags & _IOWRITING) && flush_output(stream) != 0)
    return EOF;

  stream->unget_buf = (char)(unsigned char)c;
  stream->has_unget = 1;
  stream->flags |= _IOREADING;
  stream->flags &= ~_IOEOF;
  return (unsigned char)stream->unget_buf;
}

/*
 * fread/fwrite return complete elements, not bytes. Check multiplication
 * overflow before touching caller memory, and handle size==0 without division.
 */
int ungetc(int c, FILE *stream) {
  flockfile(stream);
  int result = ungetc_unlocked(c, stream);
  funlockfile(stream);
  return result;
}

static unsigned long fread_unlocked(void *ptr, unsigned long size, unsigned long nmemb, FILE *stream) {
  if (size == 0 || nmemb == 0)
    return 0;
  if (nmemb > ~0UL / size) {
    errno = EOVERFLOW;
    if (stream)
      stream->flags |= _IOERROR;
    return 0;
  }
  if (!ptr || !stream_can_read(stream)) {
    if (stream)
      stream->flags |= _IOERROR;
    if (!ptr)
      errno = EINVAL;
    return 0;
  }

  unsigned long bytes = size * nmemb;
  unsigned char *destination = (unsigned char *)ptr;
  unsigned long copied = 0;
  while (copied < bytes) {
    int byte = fgetc(stream);
    if (byte == EOF)
      break;
    destination[copied++] = (unsigned char)byte;
  }
  return copied / size;
}

unsigned long fread(void *ptr, unsigned long size, unsigned long nmemb, FILE *stream) {
  flockfile(stream);
  unsigned long result = fread_unlocked(ptr, size, nmemb, stream);
  funlockfile(stream);
  return result;
}

static unsigned long fwrite_unlocked(const void *ptr, unsigned long size, unsigned long nmemb, FILE *stream) {
  if (size == 0 || nmemb == 0)
    return 0;
  if (nmemb > ~0UL / size) {
    errno = EOVERFLOW;
    if (stream)
      stream->flags |= _IOERROR;
    return 0;
  }
  if (!ptr || !stream_can_write(stream)) {
    if (stream)
      stream->flags |= _IOERROR;
    if (!ptr)
      errno = EINVAL;
    return 0;
  }

  unsigned long bytes = size * nmemb;
  const unsigned char *source = (const unsigned char *)ptr;
  unsigned long written = 0;
  while (written < bytes) {
    if (fputc(source[written], stream) == EOF)
      break;
    written++;
  }
  return written / size;
}

unsigned long fwrite(const void *ptr, unsigned long size, unsigned long nmemb, FILE *stream) {
  flockfile(stream);
  unsigned long result = fwrite_unlocked(ptr, size, nmemb, stream);
  funlockfile(stream);
  return result;
}

int feof(FILE *stream) {
  flockfile(stream);
  int result = stream ? !!(stream->flags & _IOEOF) : 0;
  funlockfile(stream);
  return result;
}

int ferror(FILE *stream) {
  flockfile(stream);
  int result = stream ? !!(stream->flags & _IOERROR) : 0;
  funlockfile(stream);
  return result;
}

void clearerr(FILE *stream) {
  flockfile(stream);
  if (stream)
    stream->flags &= ~(_IOERROR | _IOEOF);
  funlockfile(stream);
}

static int fputs_unlocked(const char *string, FILE *stream) {
  if (!string || !stream_can_write(stream)) {
    if (!string)
      errno = EINVAL;
    if (stream)
      stream->flags |= _IOERROR;
    return EOF;
  }

  while (*string) {
    if (fputc((unsigned char)*string++, stream) == EOF)
      return EOF;
  }
  return 0;
}

int fputs(const char *string, FILE *stream) {
  flockfile(stream);
  int result = fputs_unlocked(string, stream);
  funlockfile(stream);
  return result;
}

int puts(const char *string) {
  if (fputs(string, stdout) == EOF || fputc('\n', stdout) == EOF)
    return EOF;
  return 0;
}

void perror(const char *prefix) {
  int saved_errno = errno;
  const char *message = strerror(saved_errno);

  if (prefix && *prefix) {
    (void)fputs(prefix, stderr);
    (void)fputs(": ", stderr);
  }
  (void)fputs(message, stderr);
  (void)fputc('\n', stderr);

  /* A failed diagnostic write must not replace the error being reported. */
  errno = saved_errno;
}

static int parse_open_mode(const char *mode, int *open_flags, int *stream_flags) {
  if (!mode || !mode[0])
    return 0;

  int plus = 0;
  int binary = 0;
  /* Validate every mode character instead of silently ignoring unknown flags. */
  for (const char *p = mode + 1; *p; p++) {
    if (*p == '+' && !plus)
      plus = 1;
    else if (*p == 'b' && !binary)
      binary = 1;
    else
      return 0;
  }

  switch (mode[0]) {
  case 'r':
    /* Read mode requires an existing file and does not truncate it. */
    *open_flags = plus ? O_RDWR : O_RDONLY;
    *stream_flags = plus ? (_IOREAD | _IOWRITE) : _IOREAD;
    return 1;
  case 'w':
    /* The kernel performs create/truncate as part of open(2). */
    *open_flags = (plus ? O_RDWR : O_WRONLY) | O_CREAT | O_TRUNC;
    *stream_flags = plus ? (_IOREAD | _IOWRITE) : _IOWRITE;
    return 1;
  case 'a':
    /* O_APPEND makes writes go to EOF even when the descriptor was seeked. */
    *open_flags = (plus ? O_RDWR : O_WRONLY) | O_CREAT | O_APPEND;
    *stream_flags = plus ? (_IOREAD | _IOWRITE) : _IOWRITE;
    return 1;
  default:
    return 0;
  }
}

FILE *fopen(const char *filename, const char *mode) {
  int open_flags;
  int stream_flags;
  if (!filename || !parse_open_mode(mode, &open_flags, &stream_flags)) {
    errno = EINVAL;
    return 0;
  }

  long fd = __syscall_result(syscall3(SYS_open, (long)filename, open_flags, 0666));
  if (fd < 0)
    return 0;

  /*
   * The kernel descriptor and user-space FILE/buffer have separate lifetimes.
   * If allocation fails after open, close the descriptor before returning.
   */
  FILE *stream = (FILE *)malloc(sizeof(*stream));
  if (!stream) {
    int saved_errno = ENOMEM;
    (void)__syscall_result(syscall1(SYS_close, fd));
    errno = saved_errno;
    return 0;
  }

  char *buffer = (char *)malloc(BUFSIZ);
  if (!buffer) {
    int saved_errno = ENOMEM;
    (void)__syscall_result(syscall1(SYS_close, fd));
    free(stream);
    errno = saved_errno;
    return 0;
  }

  *stream = (FILE){
      .fd = (int)fd,
      .buf = buffer,
      .buf_size = BUFSIZ,
      .mode = _IOFBF,
      .flags = stream_flags | _IOOWNED | _IOBUFOWNED,
      .next = open_streams,
  };
  open_streams = stream;
  return stream;
}

int fclose(FILE *stream) {
  flockfile(stream);
  if (!stream_valid(stream)) {
    funlockfile(stream);
    return EOF;
  }

  int result = 0;
  int saved_errno = 0;
  /* Preserve a flush error but still close: fclose must release the fd. */
  if (stream->flags & _IOWRITING) {
    if (flush_output(stream) != 0) {
      result = EOF;
      saved_errno = errno;
    }
  }

  long close_result = __syscall_result(syscall1(SYS_close, stream->fd));
  if (close_result < 0 && result == 0) {
    result = EOF;
    saved_errno = errno;
  }

  if (stream->flags & _IOOWNED) {
    /* Unlink before freeing so fflush(NULL) never traverses a dead FILE. */
    FILE **link = &open_streams;
    while (*link && *link != stream)
      link = &(*link)->next;
    if (*link == stream)
      *link = stream->next;
    char *buffer = (stream->flags & _IOBUFOWNED) ? stream->buf : 0;
    funlockfile(stream);
    free(buffer);
    free(stream);
  } else {
    stream->fd = -1;
    stream->flags &= ~(_IOREAD | _IOWRITE | _IOREADING | _IOWRITING);
    funlockfile(stream);
  }

  if (result == EOF)
    errno = saved_errno;
  return result;
}

/*
 * A parsed conversion is kept as data until emission. Flags, width, and
 * precision control presentation; length and conversion select the va_arg
 * type. Separating those concerns makes each conversion branch easier to read.
 */
typedef struct {
  int left; /* '-' flag: pad after the converted value */
  int zero; /* '0' flag: use zero padding when right aligned */
  int plus; /* '+' flag: show sign on nonnegative signed conversions */
  int space; /* ' ' flag: prefix a positive signed value with a blank */
  int alternate; /* '#' flag: request a base marker or decimal point */
  int width; /* Minimum field width; smaller values never truncate output */
  int precision; /* Conversion-specific digit or string-byte limit */
  int has_precision; /* Distinguishes omitted precision from explicit .0 */
  int length; /* Selects the promoted variadic argument width */
} format_spec;

/*
 * Parsed length modifiers; values determine the variadic type consumed for a
 * conversion. These are parser states, not the textual modifier characters.
 */
enum {
  LEN_DEFAULT, /* No modifier, usually int or unsigned int */
  LEN_LONG, /* l: long or wide-character/string form */
  LEN_LLONG, /* ll: long long */
  LEN_SIZE, /* z: size_t or its signed counterpart */
  LEN_LDOUBLE /* L: recognized but unsupported in formatted output */
};

static int emit_padding(FILE *stream, char byte, int count, int *total);

static unsigned long long power10(unsigned int exponent) {
  /* The caller caps precision first, keeping this decimal scale in 64 bits. */
  unsigned long long value = 1;
  while (exponent--)
    value *= 10;
  return value;
}

static int unsigned_decimal(double value, char *buffer, double *fraction) {
  char reversed[320];
  int count = 0;
  if (value < 1.0) {
    buffer[0] = '0';
    *fraction = value;
    return 1;
  }

  /* Find the leading decimal place, then peel off digits from most to least. */
  double factor = 1.0;
  while (factor <= value / 10.0)
    factor *= 10.0;
  while (factor >= 1.0 && count < (int)sizeof(reversed)) {
    int digit = (int)(value / factor);
    if (digit > 9)
      digit = 9;
    reversed[count++] = (char)('0' + digit);
    value -= (double)digit * factor;
    factor /= 10.0;
  }
  for (int i = 0; i < count; i++)
    buffer[i] = reversed[i];
  *fraction = value;
  return count;
}

static void increment_decimal(char *digits, int *count) {
  /* Carry left across 9s; a value such as 99 needs a new leading digit. */
  int at = *count - 1;
  while (at >= 0 && digits[at] == '9')
    digits[at--] = '0';
  if (at >= 0) {
    digits[at]++;
    return;
  }
  for (int i = *count; i > 0; i--)
    digits[i] = digits[i - 1];
  digits[0] = '1';
  (*count)++;
}

/*
 * Convert a double to the teaching formatter's decimal subset. Precision is
 * capped at 18 fractional/significant digits so the rounding scale fits in a
 * 64-bit integer. Decimal extraction is intentionally clear rather than a
 * correctly-rounded, arbitrary-precision dtoa implementation.
 */
static int format_floating(double value, char conversion, int precision, int alternate, char *buffer) {
  int uppercase = conversion == 'F' || conversion == 'E' || conversion == 'G';
  char lower = uppercase ? (char)(conversion + ('a' - 'A')) : conversion;
  union {
    double value;
    unsigned long long bits;
  } representation = {.value = value};
  int negative = value < 0.0 || (value == 0.0 && (representation.bits >> 63));
  if (negative)
    value = -value;
  int at = 0;
  if (negative)
    buffer[at++] = '-';

  if (value != value) {
    const char *word = uppercase ? "NAN" : "nan";
    while (*word)
      buffer[at++] = *word++;
    return at;
  }
  if (value > 1.7976931348623157e308) {
    const char *word = uppercase ? "INF" : "inf";
    while (*word)
      buffer[at++] = *word++;
    return at;
  }

  if (lower == 'f') {
    unsigned long long scale = power10((unsigned int)precision);
    char integer_digits[320];
    double fraction;
    int integer_count = unsigned_decimal(value, integer_digits, &fraction);
    unsigned long long fractional = (unsigned long long)(fraction * (double)scale + 0.5);
    if (fractional >= scale && scale) {
      fractional = 0;
      increment_decimal(integer_digits, &integer_count);
    }
    for (int i = 0; i < integer_count; i++)
      buffer[at++] = integer_digits[i];
    if (precision > 0 || alternate)
      buffer[at++] = '.';
    if (precision > 0) {
      unsigned long long place = scale / 10;
      for (int i = 0; i < precision; i++) {
        unsigned int digit = (unsigned int)(fractional / place);
        buffer[at++] = (char)('0' + digit);
        fractional %= place;
        if (place > 1)
          place /= 10;
      }
    }
    return at;
  }

  int exponent = 0;
  double normalized = value;
  if (normalized != 0.0) {
    while (normalized >= 10.0 && exponent < 400) {
      normalized /= 10.0;
      exponent++;
    }
    while (normalized < 1.0 && exponent > -400) {
      normalized *= 10.0;
      exponent--;
    }
  }

  if (lower == 'g') {
    /*
     * %g chooses fixed or exponent notation from the decimal exponent and the
     * requested significant digits; in fixed notation, adjust fractional
     * precision so the total significant-digit count stays consistent.
     */
    int significant = precision ? precision : 1;
    lower = (exponent < -4 || exponent >= significant) ? 'e' : 'f';
    if (lower == 'f')
      precision = significant - (exponent + 1);
    else
      precision = significant - 1;
    if (precision < 0)
      precision = 0;
  }

  if (lower == 'e') {
    unsigned long long scale = power10((unsigned int)precision);
    unsigned long long digits = (unsigned long long)(normalized * (double)scale + 0.5);
    unsigned long long limit = scale * 10ULL;
    if (digits >= limit) {
      digits /= 10;
      exponent++;
    }
    unsigned long long leading_place = scale;
    buffer[at++] = (char)('0' + (unsigned int)(digits / leading_place));
    digits %= leading_place;
    if (precision > 0 || alternate)
      buffer[at++] = '.';
    for (int i = 0; i < precision; i++) {
      leading_place /= 10;
      buffer[at++] = (char)('0' + (unsigned int)(digits / leading_place));
      digits %= leading_place;
    }
    buffer[at++] = uppercase ? 'E' : 'e';
    buffer[at++] = exponent < 0 ? '-' : '+';
    if (exponent < 0)
      exponent = -exponent;
    if (exponent >= 100)
      buffer[at++] = (char)('0' + exponent / 100);
    if (exponent >= 10)
      buffer[at++] = (char)('0' + (exponent / 10) % 10);
    else
      buffer[at++] = '0';
    buffer[at++] = (char)('0' + exponent % 10);
  } else {
    unsigned long long scale = power10((unsigned int)precision);
    double fraction;
    char integer_digits[320];
    int integer_count = unsigned_decimal(value, integer_digits, &fraction);
    unsigned long long fractional = (unsigned long long)(fraction * (double)scale + 0.5);
    if (fractional >= scale && scale) {
      fractional = 0;
      increment_decimal(integer_digits, &integer_count);
    }
    for (int i = 0; i < integer_count; i++)
      buffer[at++] = integer_digits[i];
    if (precision > 0 || alternate)
      buffer[at++] = '.';
    if (precision > 0) {
      unsigned long long place = scale / 10;
      for (int i = 0; i < precision; i++) {
        buffer[at++] = (char)('0' + (unsigned int)(fractional / place));
        fractional %= place;
        if (place > 1)
          place /= 10;
      }
    }
  }

  if ((conversion == 'g' || conversion == 'G') && !alternate) {
    /* Remove fractional trailing zeroes, but preserve integer zeroes (10)
     * and the exponent itself (e+01). */
    int exponent_at = at;
    while (exponent_at > 0 && buffer[exponent_at - 1] != 'e' && buffer[exponent_at - 1] != 'E')
      exponent_at--;
    int mantissa_end = exponent_at ? exponent_at - 1 : at;
    int decimal_at = -1;
    for (int i = 0; i < mantissa_end; i++) {
      if (buffer[i] == '.') {
        decimal_at = i;
        break;
      }
    }
    while (decimal_at >= 0 && mantissa_end > decimal_at + 1 && buffer[mantissa_end - 1] == '0')
      mantissa_end--;
    if (decimal_at >= 0 && mantissa_end == decimal_at + 1)
      mantissa_end--;
    if (exponent_at) {
      int exponent_start = exponent_at - 1;
      int exponent_length = at - exponent_start;
      for (int i = 0; i < exponent_length; i++)
        buffer[mantissa_end + i] = buffer[exponent_start + i];
      at = mantissa_end + exponent_length;
    } else {
      at = mantissa_end;
    }
  }
  return at;
}

static int emit_float(FILE *stream, const char *text, int length, format_spec spec, int *total) {
  int prefix = text[0] == '-' || spec.plus || spec.space;
  int padding = spec.width > length + (prefix && text[0] != '-' ? 1 : 0)
                    ? spec.width - length - (prefix && text[0] != '-' ? 1 : 0)
                    : 0;
  int sign_length = text[0] == '-' ? 1 : 0;
  if (!spec.left && !spec.zero && emit_padding(stream, ' ', padding, total) != 0)
    return -1;
  for (int i = 0; i < sign_length; i++) {
    if (fputc(text[i], stream) == EOF)
      return -1;
    if (*total == INT_MAX_VALUE) {
      errno = EOVERFLOW;
      return -1;
    }
    (*total)++;
  }
  if (!sign_length && prefix) {
    if (fputc(spec.plus ? '+' : ' ', stream) == EOF)
      return -1;
    if (*total == INT_MAX_VALUE) {
      errno = EOVERFLOW;
      return -1;
    }
    (*total)++;
  }
  if (!spec.left && spec.zero && emit_padding(stream, '0', padding, total) != 0)
    return -1;
  for (int i = sign_length; i < length; i++) {
    if (fputc(text[i], stream) == EOF)
      return -1;
    if (*total == INT_MAX_VALUE) {
      errno = EOVERFLOW;
      return -1;
    }
    (*total)++;
  }
  if (spec.left && emit_padding(stream, ' ', padding, total) != 0)
    return -1;
  return 0;
}

static int emit_padding(FILE *stream, char byte, int count, int *total) {
  while (count-- > 0) {
    if (fputc(byte, stream) == EOF)
      return -1;
    if (*total == INT_MAX_VALUE) {
      errno = EOVERFLOW;
      return -1;
    }
    (*total)++;
  }
  return 0;
}

static int emit_text(FILE *stream, const char *text, int length, format_spec spec, int *total) {
  int padding = spec.width > length ? spec.width - length : 0;
  if (!spec.left && emit_padding(stream, ' ', padding, total) != 0)
    return -1;
  for (int i = 0; i < length; i++) {
    if (fputc((unsigned char)text[i], stream) == EOF)
      return -1;
    if (*total == INT_MAX_VALUE) {
      errno = EOVERFLOW;
      return -1;
    }
    (*total)++;
  }
  if (spec.left && emit_padding(stream, ' ', padding, total) != 0)
    return -1;
  return 0;
}

static int emit_number(FILE *stream, unsigned long value, int negative, unsigned int base, int uppercase,
                       int allow_sign, format_spec spec, int *total) {
  char digits[sizeof(unsigned long) * 8 + 1];
  int digit_count = 0;
  const char *alphabet = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
  int nonzero = value != 0;
  /*
   * Repeated division discovers least-significant digits first. Save them,
   * then emit in reverse so the printed digits have their usual order.
   */
  if (value != 0 || !spec.has_precision || spec.precision != 0) {
    do {
      digits[digit_count++] = alphabet[value % base];
      value /= base;
    } while (value != 0);
  }

  int precision_zeros = spec.has_precision && spec.precision > digit_count ? spec.precision - digit_count : 0;
  char prefix[2];
  int prefix_length = 0;
  if (spec.alternate && base == 8 && precision_zeros == 0 &&
      (digit_count == 0 || digits[digit_count - 1] != '0')) {
    prefix[prefix_length++] = '0';
  } else if (spec.alternate && base == 16 && nonzero) {
    prefix[prefix_length++] = '0';
    prefix[prefix_length++] = uppercase ? 'X' : 'x';
  }
  char sign = negative ? '-' : allow_sign && spec.plus ? '+' : allow_sign && spec.space ? ' ' : '\0';
  int number_length = digit_count + precision_zeros + prefix_length + (sign != '\0');
  int padding = spec.width > number_length ? spec.width - number_length : 0;
  char pad = spec.zero && !spec.left && !spec.has_precision ? '0' : ' ';

  if (!spec.left && pad == ' ' && emit_padding(stream, ' ', padding, total) != 0)
    return -1;
  if (sign) {
    if (fputc(sign, stream) == EOF)
      return -1;
    if (*total == INT_MAX_VALUE) {
      errno = EOVERFLOW;
      return -1;
    }
    (*total)++;
  }
  for (int i = 0; i < prefix_length; i++) {
    if (fputc(prefix[i], stream) == EOF)
      return -1;
    if (*total == INT_MAX_VALUE) {
      errno = EOVERFLOW;
      return -1;
    }
    (*total)++;
  }
  if (!spec.left && pad == '0' && emit_padding(stream, '0', padding, total) != 0)
    return -1;
  if (emit_padding(stream, '0', precision_zeros, total) != 0)
    return -1;
  while (digit_count > 0) {
    if (fputc(digits[--digit_count], stream) == EOF)
      return -1;
    if (*total == INT_MAX_VALUE) {
      errno = EOVERFLOW;
      return -1;
    }
    (*total)++;
  }
  if (spec.left && emit_padding(stream, ' ', padding, total) != 0)
    return -1;
  return 0;
}

static long long signed_argument(__builtin_va_list args, int length) {
  if (length == LEN_LONG)
    return __builtin_va_arg(args, long);
  if (length == LEN_LLONG)
    return __builtin_va_arg(args, long long);
  if (length == LEN_SIZE)
    return (long long)__builtin_va_arg(args, long);
  return __builtin_va_arg(args, int);
}

static unsigned long long unsigned_argument(__builtin_va_list args, int length) {
  if (length == LEN_LONG)
    return __builtin_va_arg(args, unsigned long);
  if (length == LEN_LLONG)
    return __builtin_va_arg(args, unsigned long long);
  if (length == LEN_SIZE)
    return __builtin_va_arg(args, unsigned long);
  return __builtin_va_arg(args, unsigned int);
}

static int vformat_unlocked(FILE *stream, const char *format, __builtin_va_list args) {
  int total = 0;
  if (!format || !stream_can_write(stream)) {
    if (!format)
      errno = EINVAL;
    return -1;
  }

  while (*format) {
    if (*format != '%') {
      if (fputc((unsigned char)*format++, stream) == EOF)
        return -1;
      if (total == INT_MAX_VALUE) {
        errno = EOVERFLOW;
        return -1;
      }
      total++;
      continue;
    }
    format++;
    if (*format == '%') {
      if (fputc('%', stream) == EOF)
        return -1;
      if (total == INT_MAX_VALUE) {
        errno = EOVERFLOW;
        return -1;
      }
      total++;
      format++;
      continue;
    }

    /*
     * Parse the conversion grammar in order: flags, width, precision, length,
     * and finally the conversion letter. Later code uses this record to choose
     * both the variadic argument type and its presentation.
     */
    format_spec spec = {0};
    spec.length = LEN_DEFAULT;
    int parsing = 1;
    while (parsing) {
      switch (*format) {
      case '-':
        spec.left = 1;
        format++;
        break;
      case '0':
        spec.zero = 1;
        format++;
        break;
      case '+':
        spec.plus = 1;
        format++;
        break;
      case ' ':
        spec.space = 1;
        format++;
        break;
      case '#':
        spec.alternate = 1;
        format++;
        break;
      default:
        parsing = 0;
        break;
      }
    }

    if (*format == '*') {
      /* A negative dynamic width means left alignment and positive width. */
      spec.width = __builtin_va_arg(args, int);
      if (spec.width < 0) {
        if (spec.width == (-INT_MAX_VALUE - 1)) {
          errno = EOVERFLOW;
          return -1;
        }
        spec.left = 1;
        spec.width = -spec.width;
      }
      format++;
    } else {
      while (*format >= '0' && *format <= '9') {
        if (spec.width > (INT_MAX_VALUE - 9) / 10) {
          errno = EOVERFLOW;
          return -1;
        }
        spec.width = spec.width * 10 + (*format++ - '0');
      }
    }

    if (*format == '.') {
      spec.has_precision = 1;
      format++;
      if (*format == '*') {
        /* A negative dynamic precision means precision was not specified. */
        spec.precision = __builtin_va_arg(args, int);
        if (spec.precision < 0)
          spec.has_precision = 0;
        format++;
      } else {
        while (*format >= '0' && *format <= '9') {
          if (spec.precision > (INT_MAX_VALUE - 9) / 10) {
            errno = EOVERFLOW;
            return -1;
          }
          spec.precision = spec.precision * 10 + (*format++ - '0');
        }
      }
    }

    if (*format == 'l') {
      spec.length = LEN_LONG;
      format++;
      if (*format == 'l') {
        spec.length = LEN_LLONG;
        format++;
      }
    } else if (*format == 'z') {
      spec.length = LEN_SIZE;
      format++;
    } else if (*format == 'h') {
      format++;
      if (*format == 'h')
        format++;
    } else if (*format == 'L') {
      spec.length = LEN_LDOUBLE;
      format++;
    }

    char conversion = *format;
    if (!conversion) {
      errno = EINVAL;
      return -1;
    }
    format++;

    if (conversion == 's') {
      if (spec.length == LEN_LONG) {
        const wchar_t *wide = __builtin_va_arg(args, const wchar_t *);
        char text[1024];
        int length = 0;
        if (!wide) {
          wide = (const wchar_t[]){'(', 'n', 'u', 'l', 'l', ')', 0};
        }
        while (wide[length] && (!spec.has_precision || length < spec.precision)) {
          if ((unsigned long)wide[length] > 0x7fUL || length >= (int)sizeof(text)) {
            errno = length >= (int)sizeof(text) ? EOVERFLOW : EILSEQ;
            return -1;
          }
          text[length] = (char)wide[length];
          length++;
        }
        if (emit_text(stream, text, length, spec, &total) != 0)
          return -1;
        continue;
      }
      const char *text = __builtin_va_arg(args, const char *);
      if (!text)
        text = "(null)";
      int length = 0;
      /* For %s, precision limits bytes copied; width is the minimum field size. */
      while (text[length] && (!spec.has_precision || length < spec.precision))
        length++;
      if (emit_text(stream, text, length, spec, &total) != 0)
        return -1;
    } else if (conversion == 'c') {
      if (spec.length == LEN_LONG) {
        unsigned long wide = (unsigned long)__builtin_va_arg(args, unsigned int);
        if (wide > 0x7fUL) {
          errno = EILSEQ;
          return -1;
        }
        char byte = (char)wide;
        if (emit_text(stream, &byte, 1, spec, &total) != 0)
          return -1;
        continue;
      }
      char byte = (char)__builtin_va_arg(args, int);
      if (emit_text(stream, &byte, 1, spec, &total) != 0)
        return -1;
    } else if (conversion == 'd' || conversion == 'i') {
      long long value = signed_argument(args, spec.length);
      int negative = value < 0;
      /* Unsigned subtraction obtains |LONG_MIN| without signed overflow. */
      unsigned long magnitude = negative ? 0UL - (unsigned long)value : (unsigned long)value;
      if (emit_number(stream, magnitude, negative, 10, 0, 1, spec, &total) != 0)
        return -1;
    } else if (conversion == 'u' || conversion == 'o' || conversion == 'x' || conversion == 'X') {
      unsigned long value = (unsigned long)unsigned_argument(args, spec.length);
      unsigned int base = conversion == 'u' ? 10 : conversion == 'o' ? 8 : 16;
      if (emit_number(stream, value, 0, base, conversion == 'X', 0, spec, &total) != 0)
        return -1;
    } else if (conversion == 'p') {
      unsigned long value = (unsigned long)__builtin_va_arg(args, void *);
      char digits[sizeof(unsigned long) * 2];
      int digit_count = 0;
      do {
        int digit = value & 15;
        digits[digit_count++] = "0123456789abcdef"[digit];
        value >>= 4;
      } while (value);
      int padding = spec.width > digit_count + 2 ? spec.width - digit_count - 2 : 0;
      if (!spec.left && !spec.zero && emit_padding(stream, ' ', padding, &total) != 0)
        return -1;
      if (fputc('0', stream) == EOF || fputc('x', stream) == EOF)
        return -1;
      if (total > INT_MAX_VALUE - 2) {
        errno = EOVERFLOW;
        return -1;
      }
      total += 2;
      if (!spec.left && spec.zero && emit_padding(stream, '0', padding, &total) != 0)
        return -1;
      while (digit_count > 0) {
        if (fputc(digits[--digit_count], stream) == EOF)
          return -1;
        if (total == INT_MAX_VALUE) {
          errno = EOVERFLOW;
          return -1;
        }
        total++;
      }
      if (spec.left && emit_padding(stream, ' ', padding, &total) != 0)
        return -1;
    } else if (conversion == 'f' || conversion == 'F' || conversion == 'e' || conversion == 'E' || conversion == 'g' ||
               conversion == 'G') {
      if (spec.length == LEN_LDOUBLE) {
        (void)__builtin_va_arg(args, long double);
        errno = ENOSYS;
        return -1;
      }
      double value = __builtin_va_arg(args, double);
      int precision = spec.has_precision ? spec.precision : 6;
      if (precision > 18) {
        errno = EOVERFLOW;
        return -1;
      }
      char text[384];
      int length = format_floating(value, conversion, precision, spec.alternate, text);
      if (emit_float(stream, text, length, spec, &total) != 0)
        return -1;
    } else {
      /* Unknown conversions are rejected instead of silently dropping output. */
      errno = EINVAL;
      return -1;
    }
  }
  return total;
}

static int vformat(FILE *stream, const char *format, __builtin_va_list args) {
  flockfile(stream);
  int result = vformat_unlocked(stream, format, args);
  funlockfile(stream);
  return result;
}

int vfprintf(FILE *stream, const char *format, __builtin_va_list args) { return vformat(stream, format, args); }

int fprintf(FILE *stream, const char *format, ...) {
  __builtin_va_list args;
  __builtin_va_start(args, format);
  int result = vformat(stream, format, args);
  __builtin_va_end(args);
  return result;
}

int vprintf(const char *format, __builtin_va_list args) { return vfprintf(stdout, format, args); }

int printf(const char *format, ...) {
  __builtin_va_list args;
  __builtin_va_start(args, format);
  int result = vformat(stdout, format, args);
  __builtin_va_end(args);
  return result;
}

int vsnprintf(char *buffer, unsigned long size, const char *format, __builtin_va_list args) {
  if ((!buffer && size != 0) || !format) {
    errno = EINVAL;
    return -1;
  }

  if (size > (unsigned long)INT_MAX_VALUE) {
    errno = EOVERFLOW;
    return -1;
  }
  FILE sink = {
      .fd = 1,
      .buf = buffer,
      .buf_size = size ? (int)(size - 1) : 0,
      .mode = _IONBF,
      .flags = _IOWRITE,
      .string_mode = 1,
  };
  int result = vformat(&sink, format, args);
  if (size != 0)
    buffer[sink.string_count < size ? sink.string_count : size - 1] = '\0';
  return result;
}

int snprintf(char *buffer, unsigned long size, const char *format, ...) {
  __builtin_va_list args;
  __builtin_va_start(args, format);
  int result = vsnprintf(buffer, size, format, args);
  __builtin_va_end(args);
  return result;
}

static int is_space(char byte) {
  return byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r' || byte == '\v' || byte == '\f';
}

static int digit_value(char byte) {
  if (byte >= '0' && byte <= '9')
    return byte - '0';
  if (byte >= 'a' && byte <= 'f')
    return byte - 'a' + 10;
  if (byte >= 'A' && byte <= 'F')
    return byte - 'A' + 10;
  return -1;
}

static int scanset_contains(const char *set, int negate, unsigned char byte) {
  int matched = 0;
  /* A hyphen between two set characters denotes an inclusive byte range. */
  for (int i = 0; set[i]; i++) {
    if (set[i] == '-' && i > 0 && set[i + 1] && set[i + 1] != ']') {
      unsigned char first = (unsigned char)set[i - 1];
      unsigned char last = (unsigned char)set[i + 1];
      if (first <= byte && byte <= last)
        matched = 1;
      i++;
    } else if ((unsigned char)set[i] == byte) {
      matched = 1;
    }
  }
  return negate ? !matched : matched;
}

/*
 * String scanner shared by sscanf and the seekable-file fscanf implementation.
 * input_consumed reports how far the format matched so fscanf can put unread
 * bytes back by restoring the file position.
 */
static int vscan_string(const char *input, const char *format, __builtin_va_list args, int *input_consumed) {
  if (!input || !format) {
    errno = EINVAL;
    return EOF;
  }

  int assigned = 0;
  int consumed = 0;
  int input_failure = 0;

  /*
   * Scanning is a sequence of matches, not tokenization of the whole string:
   * the first mismatch stops the operation and leaves later input untouched.
   */
  while (*format) {
    if (is_space(*format)) {
      /* Any run of format whitespace matches any run (including empty) in input. */
      while (is_space(*format))
        format++;
      while (is_space(input[consumed]))
        consumed++;
      continue;
    }
    if (*format != '%') {
      if (input[consumed] != *format) {
        input_failure = input[consumed] == '\0';
        break;
      }
      consumed++;
      format++;
      continue;
    }
    format++;
    if (*format == '%') {
      if (input[consumed] != '%')
        break;
      consumed++;
      format++;
      continue;
    }

    int suppress = 0;
    if (*format == '*') {
      /* Suppression consumes input but does not fetch an output pointer. */
      suppress = 1;
      format++;
    }
    int width = 0;
    while (*format >= '0' && *format <= '9') {
      if (width > (INT_MAX_VALUE - 9) / 10) {
        errno = EOVERFLOW;
        if (input_consumed)
          *input_consumed = consumed;
        return assigned;
      }
      width = width * 10 + (*format++ - '0');
    }
    if (width == 0)
      width = INT_MAX_VALUE;

    int length = LEN_DEFAULT;
    if (*format == 'l') {
      length = LEN_LONG;
      format++;
      if (*format == 'l') {
        length = LEN_LLONG;
        format++;
      }
    } else if (*format == 'z') {
      length = LEN_SIZE;
      format++;
    } else if (*format == 'h') {
      format++;
      if (*format == 'h')
        format++;
    } else if (*format == 'L') {
      length = LEN_LDOUBLE;
      format++;
    }

    char conversion = *format++;
    if (!conversion)
      break;

    /* %n stores the number of input bytes consumed and adds no assignment. */
    if (conversion == 'n') {
      if (!suppress) {
        if (length == LEN_LONG || length == LEN_SIZE)
          *__builtin_va_arg(args, long *) = consumed;
        else if (length == LEN_LLONG)
          *__builtin_va_arg(args, long long *) = consumed;
        else
          *__builtin_va_arg(args, int *) = consumed;
      }
      continue;
    }

    if (conversion == 'c') {
      /* Unlike most conversions, %c does not skip leading whitespace and
       * does not append a NUL byte; its default width is exactly one. */
      wchar_t *wide_destination = 0;
      char *destination = 0;
      if (!suppress) {
        if (length == LEN_LONG)
          wide_destination = __builtin_va_arg(args, wchar_t *);
        else
          destination = __builtin_va_arg(args, char *);
      }
      int count = width == INT_MAX_VALUE ? 1 : width;
      for (int i = 0; i < count; i++) {
        if (!input[consumed]) {
          input_failure = 1;
          goto scan_done;
        }
        if (wide_destination) {
          unsigned char byte = (unsigned char)input[consumed];
          if (byte > 0x7f) {
            errno = EILSEQ;
            goto scan_done;
          }
          wide_destination[i] = (wchar_t)byte;
        } else if (destination) {
          destination[i] = input[consumed];
        }
        consumed++;
      }
      if (!suppress)
        assigned++;
      continue;
    }

    if (conversion != '[') {
      /* Numeric, %s, and other ordinary conversions skip input whitespace.
       * %c and scansets are the exceptions. */
      while (is_space(input[consumed]))
        consumed++;
    }

    if (conversion == 's') {
      wchar_t *wide_destination = 0;
      char *destination = 0;
      if (!suppress) {
        if (length == LEN_LONG)
          wide_destination = __builtin_va_arg(args, wchar_t *);
        else
          destination = __builtin_va_arg(args, char *);
      }
      int count = 0;
      while (input[consumed] && !is_space(input[consumed]) && count < width) {
        if (wide_destination) {
          unsigned char byte = (unsigned char)input[consumed];
          if (byte > 0x7f) {
            errno = EILSEQ;
            goto scan_done;
          }
          wide_destination[count] = (wchar_t)byte;
        } else if (destination) {
          destination[count] = input[consumed];
        }
        consumed++;
        count++;
      }
      if (count == 0) {
        input_failure = input[consumed] == '\0';
        goto scan_done;
      }
      if (wide_destination)
        wide_destination[count] = 0;
      else if (destination)
        destination[count] = '\0';
      if (!suppress)
        assigned++;
      continue;
    }

    if (conversion == '[') {
      int negate = 0;
      if (*format == '^') {
        negate = 1;
        format++;
      }
      /*
       * Copy the scanset out of the format so matching can interpret ranges
       * and negation without moving the format cursor during every input byte.
       */
      char set[128];
      int set_length = 0;
      if (*format == ']')
        set[set_length++] = *format++;
      while (*format && *format != ']') {
        if (set_length >= (int)sizeof(set) - 1) {
          errno = EOVERFLOW;
          if (input_consumed)
            *input_consumed = consumed;
          return assigned;
        }
        set[set_length++] = *format++;
      }
      if (*format != ']' || set_length == 0) {
        errno = EINVAL;
        if (input_consumed)
          *input_consumed = consumed;
        return assigned;
      }
      set[set_length] = '\0';
      format++;
      wchar_t *wide_destination = 0;
      char *destination = 0;
      if (!suppress) {
        if (length == LEN_LONG)
          wide_destination = __builtin_va_arg(args, wchar_t *);
        else
          destination = __builtin_va_arg(args, char *);
      }
      int count = 0;
      while (input[consumed] && count < width && scanset_contains(set, negate, (unsigned char)input[consumed])) {
        if (wide_destination) {
          unsigned char byte = (unsigned char)input[consumed];
          if (byte > 0x7f) {
            errno = EILSEQ;
            goto scan_done;
          }
          wide_destination[count] = (wchar_t)byte;
        } else if (destination) {
          destination[count] = input[consumed];
        }
        consumed++;
        count++;
      }
      if (count == 0) {
        input_failure = input[consumed] == '\0';
        goto scan_done;
      }
      if (wide_destination)
        wide_destination[count] = 0;
      else if (destination)
        destination[count] = '\0';
      if (!suppress)
        assigned++;
      continue;
    }

    if (conversion == 'f' || conversion == 'F' || conversion == 'e' || conversion == 'E' || conversion == 'g' ||
        conversion == 'G') {
      int conversion_start = consumed;
      int negative = 0;
      if (width > 0 && (input[consumed] == '+' || input[consumed] == '-')) {
        negative = input[consumed] == '-';
        consumed++;
        width--;
      }

      /*
       * Build the significand one digit at a time, then apply the exponent.
       * This transparent subset intentionally avoids locale and correctly-
       * rounded decimal conversion machinery.
       */
      double value = 0.0;
      double place = 0.1;
      int digits = 0;
      int after_decimal = 0;
      while (width > 0) {
        char byte = input[consumed];
        if (byte >= '0' && byte <= '9') {
          int digit = byte - '0';
          value = after_decimal ? value + digit * place : value * 10.0 + digit;
          if (after_decimal)
            place *= 0.1;
          consumed++;
          width--;
          digits++;
        } else if (byte == '.' && !after_decimal) {
          after_decimal = 1;
          consumed++;
          width--;
        } else {
          break;
        }
      }
      if (digits == 0) {
        input_failure = consumed == conversion_start && input[consumed] == '\0';
        goto scan_done;
      }

      int exponent = 0;
      int exponent_negative = 0;
      if (width > 0 && (input[consumed] == 'e' || input[consumed] == 'E')) {
        int exponent_start = consumed;
        int exponent_width = width;
        consumed++;
        width--;
        if (width > 0 && (input[consumed] == '+' || input[consumed] == '-')) {
          exponent_negative = input[consumed] == '-';
          consumed++;
          width--;
        }
        int exponent_digits = 0;
        while (width > 0 && input[consumed] >= '0' && input[consumed] <= '9') {
          if (exponent < 400)
            exponent = exponent * 10 + input[consumed] - '0';
          consumed++;
          width--;
          exponent_digits++;
        }
        if (!exponent_digits) {
          consumed = exponent_start;
          width = exponent_width;
          exponent = 0;
          exponent_negative = 0;
        }
      }
      if (exponent_negative)
        exponent = -exponent;
      while (exponent > 0) {
        value *= 10.0;
        exponent--;
      }
      while (exponent < 0) {
        value *= 0.1;
        exponent++;
      }
      if (negative)
        value = -value;

      if (!suppress) {
        if (length == LEN_LONG)
          *__builtin_va_arg(args, double *) = value;
        else if (length == LEN_LDOUBLE)
          *__builtin_va_arg(args, long double *) = (long double)value;
        else
          *__builtin_va_arg(args, float *) = (float)value;
        assigned++;
      }
      continue;
    }

    int base;
    int is_signed;
    if (conversion == 'd') {
      base = 10;
      is_signed = 1;
    } else if (conversion == 'i') {
      base = 0;
      is_signed = 1;
    } else if (conversion == 'u') {
      base = 10;
      is_signed = 0;
    } else if (conversion == 'o') {
      base = 8;
      is_signed = 0;
    } else if (conversion == 'x' || conversion == 'X' || conversion == 'p') {
      base = 16;
      is_signed = 0;
    } else {
      errno = EINVAL;
      break;
    }

    int conversion_start = consumed;
    int negative = 0;
    if (width > 0 && (input[consumed] == '+' || input[consumed] == '-')) {
      negative = input[consumed] == '-';
      consumed++;
      width--;
    }

    if (base == 0) {
      /* %i infers octal from a leading zero and hexadecimal from 0x. */
      base = 10;
      if (input[consumed] == '0') {
        base = 8;
        if ((input[consumed + 1] == 'x' || input[consumed + 1] == 'X') && width >= 2) {
          base = 16;
          consumed += 2;
          width -= 2;
        }
      }
    } else if (base == 16 && input[consumed] == '0' && width >= 2 &&
               (input[consumed + 1] == 'x' || input[consumed + 1] == 'X')) {
      consumed += 2;
      width -= 2;
    }

    unsigned long long value = 0;
    int digits = 0;
    while (width > 0) {
      int digit = digit_value(input[consumed]);
      if (digit < 0 || digit >= base)
        break;
      /* Check before multiply-add so overflow cannot wrap into a small value. */
      if (value > (~0ULL - (unsigned int)digit) / (unsigned int)base) {
        errno = ERANGE;
        if (input_consumed)
          *input_consumed = consumed;
        return assigned;
      }
      value = value * (unsigned int)base + (unsigned int)digit;
      consumed++;
      width--;
      digits++;
    }
    if (digits == 0) {
      input_failure = consumed == conversion_start && input[consumed] == '\0';
      goto scan_done;
    }

    /* scanf accepts a leading minus for unsigned conversions modulo the type. */
    if (!is_signed && negative)
      value = 0ULL - value;

    if (!suppress) {
      if (is_signed) {
        unsigned long long signed_limit = length == LEN_DEFAULT ? 0x7fffffffULL : 0x7fffffffffffffffULL;
        if (negative)
          signed_limit++;
        if (value > signed_limit) {
          errno = ERANGE;
          if (input_consumed)
            *input_consumed = consumed;
          return assigned;
        }
        long long signed_value;
        if (negative && length != LEN_DEFAULT && value == 0x8000000000000000ULL)
          signed_value = (long long)0x8000000000000000ULL;
        else
          signed_value = negative ? -(long long)value : (long long)value;
        if (length == LEN_LONG || length == LEN_SIZE)
          *__builtin_va_arg(args, long *) = (long)signed_value;
        else if (length == LEN_LLONG)
          *__builtin_va_arg(args, long long *) = signed_value;
        else
          *__builtin_va_arg(args, int *) = (int)signed_value;
      } else {
        if (conversion == 'p')
          *__builtin_va_arg(args, void **) = (void *)(unsigned long)value;
        else if (length == LEN_DEFAULT && value > 0xffffffffULL && !negative) {
          errno = ERANGE;
          __builtin_va_end(args);
          return assigned;
        } else if (length == LEN_LONG || length == LEN_SIZE)
          *__builtin_va_arg(args, unsigned long *) = (unsigned long)value;
        else if (length == LEN_LLONG)
          *__builtin_va_arg(args, unsigned long long *) = value;
        else
          *__builtin_va_arg(args, unsigned int *) = (unsigned int)value;
      }
      assigned++;
    }
  }

scan_done:
  if (input_consumed)
    *input_consumed = consumed;
  return input_failure && assigned == 0 ? EOF : assigned;
}

int sscanf(const char *input, const char *format, ...) {
  __builtin_va_list args;
  __builtin_va_start(args, format);
  int result = vscan_string(input, format, args, 0);
  __builtin_va_end(args);
  return result;
}

/*
 * To reuse the same scanner without duplicating conversion logic, this subset
 * buffers the remaining seekable file, scans it as a string, then restores the
 * descriptor to the first unconsumed byte. Pipes are not seekable and are not
 * yet supported by this teaching implementation.
 */
static int vfscanf_unlocked(FILE *stream, const char *format, __builtin_va_list args) {
  if (!stream || !format) {
    errno = EINVAL;
    return EOF;
  }
  long start = ftell(stream);
  if (start < 0)
    return EOF;

  /*
   * This implementation reuses the string scanner rather than maintaining a
   * second conversion engine over FILE. It therefore needs a seekable stream,
   * buffers its remaining contents, and restores the position after matching.
   */
  unsigned long capacity = 256;
  unsigned long length = 0;
  char *input = (char *)malloc(capacity);
  if (!input)
    return EOF;
  int byte;
  while ((byte = fgetc(stream)) != EOF) {
    if (length >= (unsigned long)INT_MAX_VALUE) {
      free(input);
      (void)fseek(stream, start, SEEK_SET);
      errno = EOVERFLOW;
      return EOF;
    }
    if (length + 1 >= capacity) {
      if (capacity > ~0UL / 2) {
        free(input);
        (void)fseek(stream, start, SEEK_SET);
        errno = EOVERFLOW;
        return EOF;
      }
      capacity *= 2;
      char *grown = (char *)realloc(input, capacity);
      if (!grown) {
        int saved_errno = errno;
        free(input);
        (void)fseek(stream, start, SEEK_SET);
        errno = saved_errno;
        return EOF;
      }
      input = grown;
    }
    input[length++] = (char)byte;
  }
  if (ferror(stream)) {
    int saved_errno = errno;
    free(input);
    (void)fseek(stream, start, SEEK_SET);
    errno = saved_errno;
    return EOF;
  }
  input[length] = '\0';

  int consumed = 0;
  int result = vscan_string(input, format, args, &consumed);
  int saved_errno = errno;
  free(input);
  /* The scanner may read ahead in its temporary copy; expose only matched input. */
  if (fseek(stream, start + consumed, SEEK_SET) != 0)
    return EOF;
  errno = saved_errno;
  return result;
}

int vfscanf(FILE *stream, const char *format, __builtin_va_list args) {
  flockfile(stream);
  int result = vfscanf_unlocked(stream, format, args);
  funlockfile(stream);
  return result;
}

int fscanf(FILE *stream, const char *format, ...) {
  __builtin_va_list args;
  __builtin_va_start(args, format);
  int result = vfscanf(stream, format, args);
  __builtin_va_end(args);
  return result;
}
