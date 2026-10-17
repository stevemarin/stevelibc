#include "../include/errno.h"
#include "../include/stdlib.h"

static int is_space(char c) {
  /* The six whitespace characters recognized by the C conversion functions. */
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static int digit_value(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'z')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'Z')
    return c - 'A' + 10;
  return -1;
}

/*
 * Shared integer parser. limit is the maximum magnitude for the target type;
 * parsing unsigned magnitudes lets signed conversions handle LONG_MIN without
 * overflowing while negating it. end always points to the first unconsumed
 * character, or back to the original input when no digits were accepted.
 */
static unsigned long long parse_unsigned(const char *string, char **end, int base, int *negative,
                                         unsigned long long limit, int *overflow_result) {
  const char *original = string;
  *overflow_result = 0;
  while (is_space(*string))
    string++;

  *negative = 0;
  if (*string == '+' || *string == '-') {
    *negative = *string == '-';
    string++;
  }
  if (base < 0 || base == 1 || base > 36) {
    /* No digits are consumed for an invalid base, so end points at input. */
    errno = EINVAL;
    if (end)
      *end = (char *)original;
    return 0;
  }

  if ((base == 0 || base == 16) && string[0] == '0' && (string[1] == 'x' || string[1] == 'X') &&
      digit_value(string[2]) >= 0 && digit_value(string[2]) < 16) {
    string += 2;
    base = 16;
  } else if (base == 0) {
    /* C base autodetection treats a leading zero as octal, otherwise decimal. */
    base = string[0] == '0' ? 8 : 10;
  }

  const char *digits_start = string;
  unsigned long long value = 0;
  int overflow = 0;
  for (;;) {
    int digit = digit_value(*string);
    if (digit < 0 || digit >= base)
      break;
    /* Check before multiply/add so overflow itself cannot wrap. */
    if (value > (limit - (unsigned int)digit) / (unsigned int)base) {
      overflow = 1;
      value = limit;
    } else if (!overflow) {
      value = value * (unsigned int)base + (unsigned int)digit;
    }
    string++;
  }

  if (string == digits_start) {
    /* A sign or prefix without a digit is not a successful conversion. */
    if (end)
      *end = (char *)original;
    return 0;
  }
  if (end)
    *end = (char *)string;
  if (overflow)
    errno = ERANGE;
  *overflow_result = overflow;
  return value;
}

unsigned long long strtoull(const char *string, char **end, int base) {
  int negative;
  int overflow;
  unsigned long long value = parse_unsigned(string, end, base, &negative, ~0ULL, &overflow);
  if (overflow)
    return ~0ULL;
  /* Unsigned negation is defined modulo 2^N, matching strtoull semantics. */
  return negative ? 0ULL - value : value;
}

long long strtoll(const char *string, char **end, int base) {
  int negative;
  unsigned long long positive_limit = 0x7fffffffffffffffULL;
  unsigned long long limit = positive_limit + 1ULL;

  /*
   * Signed ranges are asymmetric: |LLONG_MIN| is one greater than LLONG_MAX.
   * Select the magnitude bound before parsing, then handle the minimum value
   * explicitly because its positive counterpart is not representable.
   */
  const char *p = string;
  while (is_space(*p))
    p++;
  negative = *p == '-';
  int overflow;
  unsigned long long value =
      parse_unsigned(string, end, base, &negative, negative ? limit : positive_limit, &overflow);
  if (negative) {
    if (value == limit)
      return (-9223372036854775807LL - 1LL);
    return -(long long)value;
  }
  return (long long)value;
}

unsigned long strtoul(const char *string, char **end, int base) { return (unsigned long)strtoull(string, end, base); }

long strtol(const char *string, char **end, int base) { return (long)strtoll(string, end, base); }

int atoi(const char *string) { return (int)strtol(string, 0, 10); }

long atol(const char *string) { return strtol(string, 0, 10); }
