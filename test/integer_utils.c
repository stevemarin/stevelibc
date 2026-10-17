#include "../include/stdlib.h"

int main(void) {
  /* Absolute value retains the input type and leaves nonnegative inputs alone. */
  if (abs(-17) != 17 || abs(0) != 0 || abs(23) != 23)
    return 1;
  if (labs(-123456789L) != 123456789L || labs(0L) != 0L || labs(42L) != 42L)
    return 2;
  if (llabs(-1234567890123LL) != 1234567890123LL ||
      llabs(0LL) != 0LL || llabs(9876543210123LL) != 9876543210123LL)
    return 3;

  /*
   * Division truncates toward zero, not toward negative infinity. Therefore
   * the remainder is negative when the numerator is negative and not evenly
   * divisible.
   */
  div_t small = div(-17, 5);
  if (small.quot != -3 || small.rem != -2 || small.quot * 5 + small.rem != -17)
    return 4;
  small = div(17, -5);
  if (small.quot != -3 || small.rem != 2 || small.quot * -5 + small.rem != 17)
    return 5;
  small = div(-15, 5);
  if (small.quot != -3 || small.rem != 0)
    return 6;
  small = div(12, 4);
  if (small.quot != 3 || small.rem != 0)
    return 7;

  ldiv_t medium = ldiv(-123456789L, 10000L);
  if (medium.quot != -12345L || medium.rem != -6789L ||
      medium.quot * 10000L + medium.rem != -123456789L)
    return 8;
  medium = ldiv(123456789L, -10000L);
  if (medium.quot != -12345L || medium.rem != 6789L)
    return 9;

  lldiv_t large = lldiv(-9876543210123LL, 1000000LL);
  if (large.quot != -9876543LL || large.rem != -210123LL ||
      large.quot * 1000000LL + large.rem != -9876543210123LL)
    return 10;
  large = lldiv(9876543210123LL, -1000000LL);
  if (large.quot != -9876543LL || large.rem != 210123LL)
    return 11;

  return 0;
}
