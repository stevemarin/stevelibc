#include "../include/errno.h"
#include "../include/stdio.h"
#include "../include/string.h"

static int same(const char *left, const char *right) { return strcmp(left, right) == 0; }

static void reset_capture(FILE *stream, char *buffer, int capacity) {
  stream->fd = 999;
  stream->buf = buffer;
  stream->buf_size = capacity;
  stream->buf_pos = 0;
  stream->buf_len = 0;
  stream->mode = _IOFBF;
  stream->flags = 2;
  stream->unget_buf = 0;
  stream->has_unget = 0;
  stream->next = 0;
  stream->string_mode = 0;
  stream->string_count = 0;
  stream->lock_owner = 0;
  stream->lock_depth = 0;
  buffer[0] = '\0';
}

int main(void) {
  /* Known errors return stable text; unknown numbers have a safe fallback. */
  if (!same(strerror(0), "Success") ||
      !same(strerror(ENOENT), "No such file or directory") ||
      !same(strerror(EINVAL), "Invalid argument") ||
      !same(strerror(EOVERFLOW), "Value too large for defined data type") ||
      !same(strerror(123456), "Unknown error"))
    return 1;

  char diagnostic[96];
  FILE capture;
  reset_capture(&capture, diagnostic, sizeof(diagnostic));
  FILE *saved_stderr = stderr;
  stderr = &capture;

  errno = ENOENT;
  perror("open");
  diagnostic[capture.buf_pos] = '\0';
  if (!same(diagnostic, "open: No such file or directory\n") || errno != ENOENT)
    return 2;

  reset_capture(&capture, diagnostic, sizeof(diagnostic));
  errno = EACCES;
  perror("");
  diagnostic[capture.buf_pos] = '\0';
  if (!same(diagnostic, "Permission denied\n") || errno != EACCES)
    return 3;

  reset_capture(&capture, diagnostic, sizeof(diagnostic));
  errno = 9876;
  perror(0);
  diagnostic[capture.buf_pos] = '\0';
  if (!same(diagnostic, "Unknown error\n") || errno != 9876)
    return 4;

  reset_capture(&capture, diagnostic, sizeof(diagnostic));
  errno = 0;
  perror("status");
  diagnostic[capture.buf_pos] = '\0';
  if (!same(diagnostic, "status: Success\n") || errno != 0)
    return 5;

  stderr = saved_stderr;
  return 0;
}
