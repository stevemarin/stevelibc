#ifndef STDIO_H
#define STDIO_H

#include "./stdarg.h"

/* ==========================================================================
 * STDIO.H : Buffered Standard Input/Output Stream Definitions
 * --------------------------------------------------------------------------
 * Defines structures and constants for buffered file operations, standard
 * streams (stdin, stdout, stderr), and formatted string generation.
 * ========================================================================== */

#define EOF (-1)    // Sentinel returned when input ends or an I/O operation fails
#define BUFSIZ 1024 // Default size requested for a stream's user-space buffer

/*
 * Buffering controls when bytes move between a FILE buffer and the kernel
 * descriptor: full buffering batches writes, line buffering flushes at '\n',
 * and unbuffered mode sends/reads bytes directly.
 */
#define _IOFBF 0 // Fully buffered (flushes when buffer fills up)
#define _IOLBF 1 // Line buffered (flushes automatically on newline '\n')
#define _IONBF 2 // Unbuffered (writes immediately to kernel)

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

/*
 * FILE Stream Structure
 * Manages a descriptor, one active input/output buffer, pushback, and status.
 * This small implementation exposes its layout for freestanding tests; it is
 * not binary-compatible with another libc's FILE type.
 */
typedef struct _FILE {
  int fd; /* Underlying Linux descriptor; ownership depends on how FILE was created */
  char *buf; /* User-space staging buffer; it may be caller-owned or allocator-owned */
  int buf_size; /* Buffer capacity in bytes */
  int buf_pos; /* Next write slot or next unread byte, depending on stream direction */
  int buf_len; /* Number of valid bytes currently loaded for reading */
  int mode; /* _IOFBF, _IOLBF, or _IONBF */
  int flags; /* Access permissions and sticky read/write/EOF/error state */
  char unget_buf; /* One byte of pushback retained separately from the read buffer */
  int has_unget; /* Whether unget_buf is the next logical input byte */
  struct _FILE *next; /* Intrusive list link for libc-owned streams and fflush(NULL) */
  int string_mode; /* Internal sink flag: formatted bytes are counted, not written */
  unsigned long string_count; /* Total logical output, including bytes beyond snprintf capacity */
  int lock_owner; /* Linux TID currently owning the recursive stream lock, or zero */
  int lock_depth; /* Recursive acquisitions by lock_owner */
} FILE;

/* Conventional process streams; their descriptors are stdin=0, stdout=1, stderr=2. */
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

/*
 * FILE operations add buffering, state, and per-stream recursive locking over
 * raw descriptors. Return values follow the familiar C conventions: EOF or
 * -1 indicates failure, while counts report complete elements or bytes as
 * described by each operation. FILE's exposed layout is project-specific.
 */
/* Open a descriptor and allocate a libc-owned FILE; mode selects r/w/a, optionally with + or b. */
FILE *fopen(const char *filename, const char *mode);
/* Flush pending output and release a stream; caller-owned FILE storage is retained. */
int fclose(FILE *stream);
/* Flush one stream, or all pending output streams when stream is NULL. */
int fflush(FILE *stream);
/* Reposition a seekable stream and discard buffered bytes invalidated by the seek. */
int fseek(FILE *stream, long offset, int whence);
/* Report the logical position, accounting for buffered read-ahead and output. */
long ftell(FILE *stream);
void rewind(FILE *stream);
int fputc(int c, FILE *stream);
int fgetc(FILE *stream);
/*
 * Read at most size-1 bytes, stopping after newline or EOF, and terminate the
 * result with NUL. A final unterminated line is returned; EOF before any byte
 * returns NULL. A size of one produces an empty string without consuming input.
 */
char *fgets(char *buffer, int size, FILE *stream);
unsigned long fread(void *ptr, unsigned long size, unsigned long nmemb, FILE *stream);
unsigned long fwrite(const void *ptr, unsigned long size, unsigned long nmemb, FILE *stream);
int fputs(const char *s, FILE *stream);
int puts(const char *s);
/*
 * Write the description for the current errno to stderr. A nonempty prefix is
 * followed by ": "; the diagnostic always ends in a newline. errno is kept.
 */
void perror(const char *prefix);
int ferror(FILE *stream);
int feof(FILE *stream);
void clearerr(FILE *stream);
/* Locking is recursive so formatted functions may safely call fputc on same stream. */
void flockfile(FILE *stream);
int ftrylockfile(FILE *stream);
void funlockfile(FILE *stream);
int ungetc(int c, FILE *stream);
/* Select buffering before stream I/O; a null buffer asks libc to allocate one. */
int setvbuf(FILE *stream, char *buffer, int mode, unsigned long size);
void setbuf(FILE *stream, char *buffer);
int getchar(void);
int putchar(int c);
/*
 * Formatted output supports integer, string, character, pointer, and a
 * teaching subset of floating conversions. Counts exclude the terminating
 * NUL in string-formatting functions; snprintf always NUL-terminates when
 * size is nonzero and reports the full would-have-written count.
 */
int fprintf(FILE *stream, const char *format, ...);
int vfprintf(FILE *stream, const char *format, va_list args);
int printf(const char *format, ...);
int vprintf(const char *format, va_list args);
int snprintf(char *buffer, unsigned long size, const char *format, ...);
int vsnprintf(char *buffer, unsigned long size, const char *format, va_list args);
/*
 * Scanning reuses the string parser. fscanf is limited to seekable streams:
 * it reads ahead into temporary storage and restores the stream to the first
 * unmatched input byte. The supported conversions are a subset of ISO C.
 */
int fscanf(FILE *stream, const char *format, ...);
int vfscanf(FILE *stream, const char *format, va_list args);
int sscanf(const char *str, const char *format, ...);

#endif
