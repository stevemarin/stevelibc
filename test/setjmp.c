#include "../include/setjmp.h"

/*
 * longjmp must return to its saved setjmp point. A zero jump value is
 * normalized to one, while a nonzero value is delivered unchanged.
 */
int main(void) {
  jmp_buf environment;
  volatile int phase = 0;
  int result = setjmp(environment);

  if (phase == 0) {
    phase = 1;
    longjmp(environment, 0);
  }
  if (result != 1 || phase != 1)
    return 1;

  phase = 2;
  result = setjmp(environment);
  if (phase == 2) {
    phase = 3;
    longjmp(environment, 42);
  }
  if (result != 42 || phase != 3)
    return 2;

  return 0;
}
