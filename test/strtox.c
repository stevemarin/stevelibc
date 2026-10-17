#include "../include/errno.h"
#include "../include/stdlib.h"

static int same_text(const char *left, const char *right) {
  while (*left && *left == *right) {
    left++;
    right++;
  }
  return *left == *right;
}

static int test_unsigned_boundaries(void) {
  char *end;
  errno = 0;
  if (strtoull("18446744073709551615", &end, 10) != ~0ULL || *end || errno != 0)
    return 1;

  errno = 0;
  if (strtoull("-18446744073709551615", &end, 10) != 1 || *end || errno != 0)
    return 2;

  errno = 0;
  if (strtoull("18446744073709551616", &end, 10) != ~0ULL || *end || errno != ERANGE)
    return 3;

  /*
   * Parsing the overflowing magnitude must saturate before applying the
   * unsigned minus sign; wrapping the saturated value incorrectly yields 1.
   */
  errno = 0;
  if (strtoull("-18446744073709551616", &end, 10) != ~0ULL || *end || errno != ERANGE)
    return 4;

  return 0;
}

static int test_end_pointer_and_invalid_base(void) {
  const char *input = "  -0xg";
  char *end;
  errno = 0;
  if (strtol(input, &end, 0) != 0 || end != input + 4 || errno != 0)
    return 5;

  const char *invalid_base_input = "123";
  errno = 0;
  if (strtol(invalid_base_input, &end, 1) != 0 || end != invalid_base_input || errno != EINVAL)
    return 6;

  errno = 0;
  if (strtol("  +12tail", &end, 10) != 12 || !same_text(end, "tail") || errno != 0)
    return 7;
  return 0;
}

static int test_signed_boundaries(void) {
  char *end;
  errno = 0;
  if (strtoll("9223372036854775808", &end, 10) != 9223372036854775807LL ||
      *end || errno != ERANGE)
    return 8;

  errno = 0;
  if (strtoll("-9223372036854775809", &end, 10) != (-9223372036854775807LL - 1LL) ||
      *end || errno != ERANGE)
    return 9;
  return 0;
}

int main(void) {
  int result = test_unsigned_boundaries();
  if (result)
    return result;
  result = test_end_pointer_and_invalid_base();
  if (result)
    return result;
  return test_signed_boundaries();
}
