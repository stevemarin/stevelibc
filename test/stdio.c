#include "../include/stdio.h"
#include "../include/errno.h"
#include "../include/stddef.h"
#include "../include/string.h"
#include "../include/syscall.h"

static int call_vprintf(const char *format, ...) {
  va_list args;
  va_start(args, format);
  int result = vprintf(format, args);
  va_end(args);
  return result;
}

static void make_test_path(char *path) {
  const char prefix[] = "/tmp/stevelibc-stdio-test-";
  char digits[20];
  int digit_count = 0;
  unsigned long pid = (unsigned long)syscall0(SYS_getpid);

  const char *part = prefix;
  while (*part)
    *path++ = *part++;
  do {
    digits[digit_count++] = (char)('0' + pid % 10);
    pid /= 10;
  } while (pid);
  while (digit_count > 0)
    *path++ = digits[--digit_count];
  *path = '\0';
}

static void append_path(char *destination, const char *directory, const char *name) {
  while (*directory)
    *destination++ = *directory++;
  while (*name)
    *destination++ = *name++;
  *destination = '\0';
}

/*
 * Use caller-owned FILE objects so the checks can inspect stream state
 * without depending on terminal output or unimplemented fopen/fclose APIs.
 */
int main(void) {
  char buffer[8];
  FILE output = {
      .fd = 999,
      .buf = buffer,
      .buf_size = sizeof(buffer),
      .mode = _IOFBF,
      .flags = 2,
  };

  /* Buffered output succeeds before any system call is needed. */
  if (fputc('Q', &output) != 'Q' || output.buf_pos != 1 || buffer[0] != 'Q')
    return 1;
  if (fwrite("xy", 0, ~0UL, &output) != 0)
    return 30;
  errno = 0;
  if (fwrite("xy", ~0UL, 2, &output) != 0 || errno != EOVERFLOW)
    return 31;

  /* Flushing an invalid descriptor reports an error and preserves unwritten data. */
  errno = 0;
  if (fflush(&output) != -1 || errno != EBADF || !(output.flags & 16))
    return 2;
  if (output.buf_pos != 1 || buffer[0] != 'Q')
    return 3;

  /* A read failure sets the stream error flag, not its end-of-file flag. */
  FILE input = {
      .fd = 999,
      .buf = buffer,
      .buf_size = sizeof(buffer),
      .mode = _IOFBF,
      .flags = 1,
  };
  errno = 0;
  if (fgetc(&input) != EOF || errno != EBADF)
    return 4;
  if (!(input.flags & 16) || (input.flags & 32))
    return 5;

  FILE locking = {
      .fd = 999,
      .buf = buffer,
      .buf_size = sizeof(buffer),
      .mode = _IOFBF,
      .flags = 2,
  };
  if (ftrylockfile(&locking) != 0 || ftrylockfile(&locking) != 0)
    return 76;
  funlockfile(&locking);
  funlockfile(&locking);
  if (fputc('R', &locking) != 'R' || locking.buf_pos != 1)
    return 77;

  /*
   * Redirect printf to an in-memory FILE. Its final flush fails because the
   * test descriptor is invalid, but the formatted bytes remain inspectable.
   */
  char formatted[64];
  FILE capture = {
      .fd = 999,
      .buf = formatted,
      .buf_size = sizeof(formatted),
      .mode = _IOFBF,
      .flags = 2,
  };
  FILE *saved_stdout = stdout;
  stdout = &capture;

  int count = printf("%s:%d", "ok", -12);
  if (count != 6 || capture.buf_pos != 6)
    return 6;
  if (formatted[0] != 'o' || formatted[1] != 'k' || formatted[2] != ':' || formatted[3] != '-' ||
      formatted[4] != '1' || formatted[5] != '2')
    return 7;

  /* Exercise the other simple output helpers and common integer formats. */
  count = fprintf(&capture, ":%04x:%ld", 0x2aU, 123456789L);
  if (count != 15)
    return 8;
  if (fputs("!", &capture) != 0 || fputc('?', &capture) != '?')
    return 9;
  if (putchar('#') != '#')
    return 10;
  if (puts("end") != 0 || call_vprintf("v%d", 7) != 2)
    return 38;
  if (capture.buf_pos != 30)
    return 39;
  if (formatted[6] != ':' || formatted[7] != '0' || formatted[8] != '0' || formatted[9] != '2' ||
      formatted[10] != 'a' || formatted[11] != ':' || formatted[12] != '1' || formatted[21] != '!' ||
      formatted[22] != '?' || formatted[23] != '#' || formatted[24] != 'e' || formatted[27] != '\n' ||
      formatted[28] != 'v' || formatted[29] != '7')
    return 11;
  if (fprintf(&capture, ":%06p", (void *)0x2a) != 7 || formatted[30] != ':' || formatted[31] != '0' ||
      formatted[32] != 'x' || formatted[33] != '0' || formatted[34] != '0' || formatted[35] != '2' ||
      formatted[36] != 'a')
    return 53;
  if (fprintf(&capture, ":%o %%", 010U) != 5 || formatted[37] != ':' || formatted[38] != '1' || formatted[39] != '0' ||
      formatted[40] != ' ' || formatted[41] != '%')
    return 57;

  char string_output[32];
  int string_count = snprintf(string_output, sizeof(string_output), "%s:%04d", "n", 23);
  if (string_count != 6 || string_output[0] != 'n' || string_output[1] != ':' || string_output[2] != '0' ||
      string_output[3] != '0' || string_output[4] != '2' || string_output[5] != '3' || string_output[6] != '\0')
    return 61;
  if (snprintf(string_output, 4, "hello") != 5 || string_output[0] != 'h' || string_output[1] != 'e' ||
      string_output[2] != 'l' || string_output[3] != '\0')
    return 62;
  if (snprintf(0, 0, "abc%d", 12) != 5)
    return 63;
  if (snprintf(string_output, sizeof(string_output), "%+010d", 674312544) != 10 ||
      string_output[0] != '+' || string_output[1] != '6' || string_output[9] != '4')
    return 65;
  if (snprintf(string_output, sizeof(string_output), "%+06u", 123U) != 6 || string_output[0] != '0' ||
      string_output[5] != '3')
    return 66;
  if (snprintf(string_output, sizeof(string_output), "%#o", 010U) != 3 || string_output[0] != '0' ||
      string_output[1] != '1' || string_output[2] != '0')
    return 67;
  if (snprintf(string_output, sizeof(string_output), "%#08x", 0x2aU) != 8 || string_output[0] != '0' ||
      string_output[1] != 'x' || string_output[7] != 'a')
    return 68;
  if (snprintf(string_output, sizeof(string_output), "%#.3o", 1U) != 3 || string_output[0] != '0' ||
      string_output[1] != '0' || string_output[2] != '1')
    return 73;
  int float_count = snprintf(string_output, sizeof(string_output), "%.2f %.2e %.4g", 1.25, 12.5, 12.5);
  if (float_count != 18)
    return 64;
  if (string_output[0] != '1' || string_output[1] != '.' || string_output[2] != '2' || string_output[3] != '5' ||
      string_output[4] != ' ' || string_output[5] != '1' || string_output[6] != '.' || string_output[7] != '2' ||
      string_output[8] != '5' || string_output[9] != 'e' || string_output[10] != '+' || string_output[11] != '0' ||
      string_output[12] != '1' || string_output[13] != ' ' || string_output[14] != '1' || string_output[15] != '2' ||
      string_output[16] != '.' || string_output[17] != '5')
    return 75;
  if (snprintf(string_output, sizeof(string_output), "%#.0f %g", 2.0, 0.00001) != 8 || string_output[0] != '2' ||
      string_output[1] != '.' || string_output[2] != ' ' || string_output[3] != '1' || string_output[4] != 'e')
    return 65;
  if (snprintf(string_output, sizeof(string_output), "%.2g", 9.99) != 2 || string_output[0] != '1' ||
      string_output[1] != '0' || string_output[2] != '\0')
    return 78;
  if (snprintf(string_output, sizeof(string_output), "%.1f", -0.0) != 4 || string_output[0] != '-' ||
      string_output[1] != '0' || string_output[2] != '.' || string_output[3] != '0')
    return 79;

  wchar_t wide_text[] = {'A', 'b', 0};
  if (snprintf(string_output, sizeof(string_output), "%ls %lc", wide_text, (unsigned int)'Z') != 4 ||
      string_output[0] != 'A' || string_output[1] != 'b' || string_output[2] != ' ' || string_output[3] != 'Z' ||
      string_output[4] != '\0')
    return 66;
  errno = 0;
  if (snprintf(string_output, sizeof(string_output), "%lc", (unsigned int)0x100) != -1 || errno != EILSEQ)
    return 67;
  if (snprintf(string_output, sizeof(string_output), "%*.*s", -5, -1, "xy") != 5 ||
      string_output[0] != 'x' || string_output[1] != 'y' || string_output[2] != ' ' || string_output[4] != ' ')
    return 80;
  if (snprintf(string_output, sizeof(string_output), "%zu:%lld:%+d", 123UL, -456LL, 7) != 11 ||
      string_output[0] != '1' || string_output[3] != ':' || string_output[4] != '-' ||
      string_output[8] != ':' || string_output[9] != '+' || string_output[10] != '7')
    return 81;
  if (snprintf(string_output, sizeof(string_output), "%s|%-4c", (char *)0, 'A') != 11 ||
      string_output[0] != '(' || string_output[5] != ')' || string_output[6] != '|' ||
      string_output[7] != 'A' || string_output[10] != ' ')
    return 82;

  union {
    double value;
    unsigned long bits;
  } special_float;
  special_float.bits = 0x7ff8000000000000UL;
  double nan_value = special_float.value;
  special_float.bits = 0x7ff0000000000000UL;
  double infinity = special_float.value;
  if (snprintf(string_output, sizeof(string_output), "%F %.1e", nan_value, infinity) != 7 ||
      string_output[0] != 'N' || string_output[2] != 'N' || string_output[3] != ' ' ||
      string_output[4] != 'i' || string_output[6] != 'f')
    return 83;

  errno = 0;
  if (snprintf(string_output, sizeof(string_output), "%") != -1 || errno != EINVAL)
    return 84;
  errno = 0;
  if (snprintf(string_output, sizeof(string_output), "%q", 1) != -1 || errno != EINVAL)
    return 85;
  errno = 0;
  if (snprintf(string_output, sizeof(string_output), "%Lf", (long double)1.0) != -1 || errno != ENOSYS)
    return 86;
  errno = 0;
  if (snprintf(string_output, sizeof(string_output), "%.19f", 1.0) != -1 || errno != EOVERFLOW)
    return 87;
  errno = 0;
  if (snprintf(string_output, sizeof(string_output), "%*d", (-2147483647 - 1), 1) != -1 || errno != EOVERFLOW)
    return 88;
  errno = 0;
  if (snprintf(string_output, ~0UL, "unused") != -1 || errno != EOVERFLOW)
    return 89;
  errno = 0;
  if (snprintf(0, 1, "unused") != -1 || errno != EINVAL)
    return 90;
  errno = 0;
  if (snprintf(string_output, sizeof(string_output), (const char *)0) != -1 || errno != EINVAL)
    return 91;
  stdout = saved_stdout;

  /* setvbuf accepts caller-owned storage and rejects unknown buffering modes. */
  char caller_buffer[32];
  FILE configurable = {
      .fd = 999,
      .buf = buffer,
      .buf_size = sizeof(buffer),
      .mode = _IOFBF,
      .flags = 2,
  };
  if (setvbuf(&configurable, caller_buffer, _IOLBF, sizeof(caller_buffer)) != 0)
    return 12;
  if (configurable.buf != caller_buffer || configurable.mode != _IOLBF)
    return 13;
  errno = 0;
  if (setvbuf(&configurable, caller_buffer, 99, sizeof(caller_buffer)) != -1 || errno != EINVAL)
    return 14;
  if (fputc('B', &configurable) != 'B')
    return 92;
  errno = 0;
  if (setvbuf(&configurable, caller_buffer, _IOFBF, sizeof(caller_buffer)) != -1 || errno != EBUSY)
    return 93;
  FILE unbuffered = {
      .fd = 999,
      .buf = buffer,
      .buf_size = sizeof(buffer),
      .mode = _IOFBF,
      .flags = 2,
  };
  if (setvbuf(&unbuffered, 0, _IONBF, 0) != 0 || unbuffered.buf != 0 || unbuffered.buf_size != 0)
    return 94;
  errno = 0;
  if (setvbuf(&unbuffered, 0, _IOFBF, 0) != -1 || errno != EINVAL)
    return 95;
  FILE invalid_stream = {.fd = -1, .flags = 2};
  errno = 0;
  if (fclose(&invalid_stream) != EOF || errno != EBADF || !ferror(&invalid_stream))
    return 96;
  errno = 0;
  if (fgetc(0) != EOF || errno != EBADF)
    return 97;
  errno = 0;
  if (setvbuf(0, 0, _IONBF, 0) != -1 || errno != EINVAL)
    return 98;
  errno = 0;
  if (fputs(0, &unbuffered) != EOF || errno != EINVAL || !ferror(&unbuffered))
    return 99;

  /* sscanf handles basic strings, signed/unsigned numbers, and literals. */
  char word[8];
  int signed_value;
  unsigned int hex_value;
  char character;
  int parsed = sscanf("name -17 2a Z", "%s %d %x %c", word, &signed_value, &hex_value, &character);
  if (parsed != 4 || word[0] != 'n' || word[3] != 'e' || word[4] != '\0' || signed_value != -17 || hex_value != 0x2a ||
      character != 'Z')
    return 15;
  void *pointer = 0;
  int consumed = 0;
  if (sscanf("0x2a!", "%p%n", &pointer, &consumed) != 1 || pointer != (void *)0x2a || consumed != 4)
    return 40;
  unsigned int unsigned_value = 0;
  if (sscanf("-1", "%u", &unsigned_value) != 1 || unsigned_value != ~0U)
    return 56;
  if (sscanf("010", "%i", &signed_value) != 1 || signed_value != 8)
    return 58;
  char set_result[16];
  int set_number = 0;
  if (sscanf("alpha-57!", "%[a-z]-%d", set_result, &set_number) != 2 || set_result[0] != 'a' || set_result[4] != 'a' ||
      set_result[5] != '\0' || set_number != 57)
    return 68;
  if (sscanf("xyz!", "%3[^!]", set_result) != 1 || set_result[0] != 'x' || set_result[2] != 'z' ||
      set_result[3] != '\0')
    return 69;
  wchar_t wide_scan[8];
  if (sscanf("wide", "%ls", wide_scan) != 1 || wide_scan[0] != 'w' || wide_scan[3] != 'e' || wide_scan[4] != 0)
    return 70;
  float scanned_float = 0.0f;
  double scanned_double = 0.0;
  if (sscanf("-1.25 3.5e1", "%f %lf", &scanned_float, &scanned_double) != 2 || scanned_float < -1.251f ||
      scanned_float > -1.249f || scanned_double < 34.99 || scanned_double > 35.01)
    return 71;
  errno = 0;
  if (sscanf("2147483648", "%d", &signed_value) != 0 || errno != ERANGE)
    return 41;
  if (sscanf("", "%d", &signed_value) != EOF)
    return 42;
  signed_value = 123;
  if (sscanf("+", "%d", &signed_value) != 0 || signed_value != 123)
    return 72;
  int scan_count = -1;
  char scan_set[8];
  if (sscanf(" 17 ]abc!tail", " %*d %2[]a-z]%n", scan_set, &scan_count) != 1 ||
      scan_set[0] != ']' || scan_set[1] != 'a' || scan_set[2] != '\0' || scan_count != 6)
    return 100;
  if (sscanf("word!", "%4[^!]!", scan_set) != 1 || scan_set[0] != 'w' || scan_set[3] != 'd' ||
      scan_set[4] != '\0')
    return 101;
  double exponent_value = 0.0;
  scan_count = -1;
  if (sscanf("1.5e+z", "%lf%n", &exponent_value, &scan_count) != 1 || exponent_value < 1.49 ||
      exponent_value > 1.51 || scan_count != 3)
    return 102;
  if (sscanf("a%!", "a%%!") != 0 || sscanf("%%", "%%") != 0)
    return 103;
  errno = 0;
  if (sscanf("x", "%[", scan_set) != 0 || errno != EINVAL)
    return 104;
  errno = 0;
  if (sscanf("x", "%q", &signed_value) != 0 || errno != EINVAL)
    return 105;
  errno = 0;
  if (sscanf(0, "%d", &signed_value) != EOF || errno != EINVAL)
    return 106;
  wchar_t non_ascii_input[] = {(wchar_t)0xff, 0};
  errno = 0;
  if (sscanf((const char *)non_ascii_input, "%ls", wide_scan) != 0 || errno != EILSEQ)
    return 107;
  char null_input[] = {(char)0xff, '\0'};
  errno = 0;
  if (sscanf(null_input, "%lc", (wchar_t *)wide_scan) != 0 || errno != EILSEQ)
    return 108;

  /* Open a real file to exercise mode parsing, buffering, and element counts. */
  char directory[64];
  char test_path[80];
  char scan_path[80];
  char missing_path[80];
  make_test_path(directory);
  append_path(test_path, directory, "/data");
  append_path(scan_path, directory, "/scan");
  append_path(missing_path, directory, "/missing");
  /* Own a private directory so these predictable test names cannot overwrite
   * another user's files or follow a pre-planted symlink in /tmp. */
  if (syscall2(SYS_mkdir, (long)directory, 0700) != 0)
    return 60;
  FILE *file = fopen(test_path, "w+");
  if (!file)
    return 16;
  const unsigned char payload[] = {'A', 'B', 0, 'C'};
  if (fwrite(payload, 1, sizeof(payload), file) != sizeof(payload))
    return 17;
  if (fflush(0) != 0)
    return 18;
  /* fclose must flush output added after the explicit all-stream flush. */
  if (fputc('D', file) != 'D')
    return 43;
  if (fclose(file) != 0)
    return 19;

  file = fopen(test_path, "r");
  if (!file)
    return 20;
  /* fflush(NULL) must not discard buffered input from an individual stream. */
  if (fgetc(file) != 'A' || fflush(0) != 0 || fgetc(file) != 'B')
    return 44;
  if (fclose(file) != 0)
    return 45;

  file = fopen(test_path, "r");
  if (!file)
    return 46;
  if (ftell(file) != 0 || fgetc(file) != 'A' || ftell(file) != 1)
    return 47;
  if (fseek(file, 0, SEEK_CUR) != 0 || fgetc(file) != 'B')
    return 48;
  if (fseek(file, -1, SEEK_END) != 0 || fgetc(file) != 'D' || ftell(file) != 5)
    return 49;
  rewind(file);
  if (ftell(file) != 0 || feof(file) || fgetc(file) != 'A')
    return 50;
  if (fclose(file) != 0)
    return 51;

  file = fopen(test_path, "r");
  if (!file)
    return 52;
  /* Flushing unread input rewinds the kernel offset to the next byte. */
  if (fgetc(file) != 'A' || fflush(file) != 0 || fgetc(file) != 'B')
    return 32;
  if (fclose(file) != 0)
    return 33;

  file = fopen(test_path, "r");
  if (!file)
    return 34;
  char bytes[8] = {0};
  if (fread(bytes, 2, 3, file) != 2)
    return 21;
  if (bytes[0] != 'A' || bytes[1] != 'B' || bytes[2] != 0 || bytes[3] != 'C' || bytes[4] != 'D')
    return 22;
  if (!feof(file) || ferror(file))
    return 23;

  /* Pushback clears EOF, and the next read returns the pushed byte first. */
  if (ungetc('Z', file) != 'Z' || feof(file) || fgetc(file) != 'Z')
    return 24;
  if (fgetc(file) != EOF || !feof(file))
    return 25;
  clearerr(file);
  if (feof(file) || ferror(file))
    return 26;
  if (fclose(file) != 0)
    return 27;

  /* fgets keeps the newline, honors the caller's bound, and returns a last
   * unterminated line before reporting EOF on the following call. */
  file = fopen(scan_path, "w+");
  if (!file || fputs("first\nsecond", file) == EOF || fseek(file, 0, SEEK_SET) != 0)
    return 112;
  char line[8];
  if (fgets(line, sizeof(line), file) != line || strcmp(line, "first\n") != 0 ||
      ftell(file) != 6)
    return 113;
  if (fgets(line, 4, file) != line || strcmp(line, "sec") != 0 || ftell(file) != 9)
    return 114;
  if (fgets(line, sizeof(line), file) != line || strcmp(line, "ond") != 0 || !feof(file))
    return 115;
  if (fgets(line, sizeof(line), file) != 0)
    return 116;
  if (fclose(file) != 0)
    return 117;

  /* Empty input is EOF immediately; a read from a write-only stream is error. */
  file = fopen(scan_path, "w+");
  if (!file || fgets(line, sizeof(line), file) != 0 || !feof(file) || ferror(file))
    return 122;
  if (fclose(file) != 0)
    return 123;
  file = fopen(scan_path, "w");
  if (!file)
    return 124;
  errno = 0;
  if (fgets(line, sizeof(line), file) != 0 || errno != EBADF || !ferror(file))
    return 125;
  if (fclose(file) != 0)
    return 126;

  /* A one-byte buffer represents an empty result and must not consume input. */
  file = fopen(scan_path, "w+");
  if (!file || fputs("x", file) == EOF || fseek(file, 0, SEEK_SET) != 0)
    return 118;
  char one_byte[1] = {'?'};
  if (fgets(one_byte, sizeof(one_byte), file) != one_byte || one_byte[0] != '\0' ||
      ftell(file) != 0 || fgetc(file) != 'x')
    return 119;
  errno = 0;
  if (fgets(line, 0, file) != 0 || errno != EINVAL)
    return 120;
  if (fclose(file) != 0)
    return 121;
  (void)syscall1(SYS_unlink, (long)scan_path);

  /* Unbuffered streams use direct one-byte reads and still track EOF. */
  file = fopen(test_path, "r");
  if (!file)
    return 35;
  if (setvbuf(file, 0, _IONBF, 0) != 0 || fgetc(file) != 'A')
    return 36;
  if (fclose(file) != 0)
    return 37;

  file = fopen(scan_path, "w+");
  if (!file || fprintf(file, "42 abc-17 2.5e1") != 15 || fseek(file, 0, SEEK_SET) != 0)
    return 72;
  char scan_word[8];
  int scan_int = 0;
  double scan_double = 0.0;
  if (fscanf(file, "%d %[a-z]-%d %lf", &scan_int, scan_word, &consumed, &scan_double) != 4 || scan_int != 42 ||
      scan_word[0] != 'a' || scan_word[2] != 'c' || scan_word[3] != '\0' || consumed != 17 || scan_double < 24.99 ||
      scan_double > 25.01 || ftell(file) != 15)
    return 73;
  if (fclose(file) != 0)
    return 74;
  (void)syscall1(SYS_unlink, (long)scan_path);

  /* Invalid open modes and missing paths report useful errors. */
  errno = 0;
  if (fopen(test_path, "q") != 0 || errno != EINVAL)
    return 28;
  errno = 0;
  if (fopen(test_path, "") != 0 || errno != EINVAL)
    return 109;
  errno = 0;
  if (fopen(test_path, "r++") != 0 || errno != EINVAL)
    return 110;
  errno = 0;
  if (fopen(0, "r") != 0 || errno != EINVAL)
    return 111;
  errno = 0;
  if (fopen(missing_path, "r") != 0 || errno != ENOENT)
    return 29;

  /* Append mode always writes at end, even when the stream is seekable. */
  file = fopen(test_path, "a");
  if (!file || fputc('E', file) != 'E' || fclose(file) != 0)
    return 54;
  file = fopen(test_path, "r");
  if (!file || fseek(file, 0, SEEK_END) != 0 || ftell(file) != 6 || fclose(file) != 0)
    return 55;

  (void)syscall1(SYS_unlink, (long)test_path);
  (void)syscall1(SYS_rmdir, (long)directory);
  return 0;
}
