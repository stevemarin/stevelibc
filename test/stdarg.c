#include "../include/stdarg.h"

static int equal_text(const char *left, const char *right) {
  while (*left && *left == *right) {
    left++;
    right++;
  }
  return *left == *right;
}

/*
 * The fixed parameter tells va_start where the unnamed arguments begin. Test
 * default promotions and different argument types in the caller's exact order.
 */
static int inspect_arguments(int tag, ...) {
  va_list original;
  va_start(original, tag);

  /* A copy can be traversed independently from the original argument list. */
  va_list copy;
  va_copy(copy, original);

  int number = va_arg(original, int);
  int copied_number = va_arg(copy, int);
  double fraction = va_arg(original, double);
  double copied_fraction = va_arg(copy, double);
  const char *text = va_arg(original, const char *);
  const char *copied_text = va_arg(copy, const char *);

  va_end(copy);
  va_end(original);

  return tag == 7 && number == 42 && copied_number == 42 &&
         fraction == 2.5 && copied_fraction == 2.5 &&
         equal_text(text, "promoted") && equal_text(copied_text, "promoted");
}

/* Each va_arg request must use the type after C's default argument promotions. */
static int sum_integers(int count, ...) {
  va_list args;
  va_start(args, count);

  int total = 0;
  for (int i = 0; i < count; i++)
    total += va_arg(args, int);

  va_end(args);
  return total;
}

int main(void) {
  if (!inspect_arguments(7, (short)42, 2.5f, "promoted"))
    return 1;
  if (sum_integers(4, (char)1, (short)2, 3, 4) != 10)
    return 2;
  return 0;
}
