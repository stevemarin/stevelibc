#include "../include/errno.h"
#include "../include/stddef.h"
#include "../include/stdint.h"
#include "../include/stdio.h"
#include "../include/stdlib.h"

int *stevelibc_fuzz_errno_location(void);
long long stevelibc_fuzz_strtoll(const char *string, char **end, int base);
unsigned long long stevelibc_fuzz_strtoull(const char *string, char **end, int base);
long stevelibc_fuzz_strtol(const char *string, char **end, int base);
unsigned long stevelibc_fuzz_strtoul(const char *string, char **end, int base);

enum { MAX_INPUT_SIZE = 4096, SENTINEL_ERRNO = 12345 };

/*
 * The first byte selects a base and the rest is a NUL-terminated number.
 * Host libc provides the differential oracle; custom symbols and errno are
 * renamed into an isolated test namespace by the Makefile.
 */
static int selected_base(unsigned char selector) {
  if (selector >= '0' && selector <= '9')
    return selector - '0';
  if (selector >= 'A' && selector <= 'Z')
    return selector - 'A' + 10;
  if (selector == '!')
    return -1;
  if (selector == '?')
    return 37;
  return selector % 37;
}

static int valid_base(int base) { return base == 0 || (base >= 2 && base <= 36); }

static void check_signed_conversion(const char *input, int base) {
  char *custom_end;
  char *system_end;

  *stevelibc_fuzz_errno_location() = SENTINEL_ERRNO;
  errno = SENTINEL_ERRNO;
  long long custom_value = stevelibc_fuzz_strtoll(input, &custom_end, base);
  int custom_errno = *stevelibc_fuzz_errno_location();
  long long system_value = strtoll(input, &system_end, base);
  int system_errno = errno;
  if (custom_value != system_value ||
      (valid_base(base) && custom_end - input != system_end - input) ||
      custom_errno != system_errno) {
    fprintf(stderr, "strtoll mismatch: base=%d input=[%s] custom=%lld/%ld/%d host=%lld/%ld/%d\n",
            base, input, custom_value, custom_end - input, custom_errno,
            system_value, system_end - input, system_errno);
    __builtin_trap();
  }

  *stevelibc_fuzz_errno_location() = SENTINEL_ERRNO;
  errno = SENTINEL_ERRNO;
  long custom_long = stevelibc_fuzz_strtol(input, &custom_end, base);
  custom_errno = *stevelibc_fuzz_errno_location();
  long system_long = strtol(input, &system_end, base);
  system_errno = errno;
  if (custom_long != system_long ||
      (valid_base(base) && custom_end - input != system_end - input) ||
      custom_errno != system_errno) {
    fprintf(stderr, "strtol mismatch: base=%d input=[%s] custom=%ld/%ld/%d host=%ld/%ld/%d\n",
            base, input, custom_long, custom_end - input, custom_errno,
            system_long, system_end - input, system_errno);
    __builtin_trap();
  }
}

static void check_unsigned_conversion(const char *input, int base) {
  char *custom_end;
  char *system_end;

  *stevelibc_fuzz_errno_location() = SENTINEL_ERRNO;
  errno = SENTINEL_ERRNO;
  unsigned long long custom_value = stevelibc_fuzz_strtoull(input, &custom_end, base);
  int custom_errno = *stevelibc_fuzz_errno_location();
  unsigned long long system_value = strtoull(input, &system_end, base);
  int system_errno = errno;
  if (custom_value != system_value ||
      (valid_base(base) && custom_end - input != system_end - input) ||
      custom_errno != system_errno) {
    fprintf(stderr, "strtoull mismatch: base=%d input=[%s] custom=%llu/%ld/%d host=%llu/%ld/%d\n",
            base, input, custom_value, custom_end - input, custom_errno,
            system_value, system_end - input, system_errno);
    __builtin_trap();
  }

  *stevelibc_fuzz_errno_location() = SENTINEL_ERRNO;
  errno = SENTINEL_ERRNO;
  unsigned long custom_long = stevelibc_fuzz_strtoul(input, &custom_end, base);
  custom_errno = *stevelibc_fuzz_errno_location();
  unsigned long system_long = strtoul(input, &system_end, base);
  system_errno = errno;
  if (custom_long != system_long ||
      (valid_base(base) && custom_end - input != system_end - input) ||
      custom_errno != system_errno) {
    fprintf(stderr, "strtoul mismatch: base=%d input=[%s] custom=%lu/%ld/%d host=%lu/%ld/%d\n",
            base, input, custom_long, custom_end - input, custom_errno,
            system_long, system_end - input, system_errno);
    __builtin_trap();
  }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  char input[MAX_INPUT_SIZE + 1];
  size_t input_size = size > MAX_INPUT_SIZE + 1 ? MAX_INPUT_SIZE + 1 : size;
  int base = input_size ? selected_base(data[0]) : 0;
  size_t text_size = input_size ? input_size - 1 : 0;

  for (size_t i = 0; i < text_size; i++)
    input[i] = (char)data[i + 1];
  input[text_size] = '\0';

  check_signed_conversion(input, base);
  check_unsigned_conversion(input, base);
  return 0;
}
