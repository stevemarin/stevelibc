#include "../include/stdlib.h"

/*
 * Exercise allocation alignment, zero-initialization, overflow rejection,
 * realloc content preservation, and reuse of adjacent freed blocks.
 */
int main(void) {
  if (malloc(0) != 0 || calloc(0, 8) != 0 || calloc(8, 0) != 0)
    return 9;
  if (malloc(~0UL) != 0 || realloc(0, ~0UL) != 0)
    return 10;
  int foreign_storage = 0;
  if (realloc(&foreign_storage, 32) != 0)
    return 11;
  free(&foreign_storage);

  unsigned char *zeroed = (unsigned char *)calloc(32, 1);
  if (!zeroed || ((unsigned long)zeroed & 15UL) != 0)
    return 1;

  for (unsigned long i = 0; i < 32; i++) {
    if (zeroed[i] != 0)
      return 2;
  }

  /* The multiplication must be rejected rather than wrapping to a small size. */
  if (calloc(~0UL, 2) != 0)
    return 3;

  zeroed[0] = 0x5a;
  unsigned char *grown = (unsigned char *)realloc(zeroed, 256);
  if (!grown || grown[0] != 0x5a)
    return 4;
  if (realloc(grown, ~0UL) != 0 || grown[0] != 0x5a)
    return 12;

  unsigned char *shrunk = (unsigned char *)realloc(grown, 16);
  if (!shrunk || shrunk[0] != 0x5a)
    return 5;
  free(shrunk);

  /* Free neighboring blocks, then request their combined capacity. */
  void *first = malloc(32);
  void *second = malloc(64);
  if (!first || !second)
    return 6;
  free(first);
  free(second);

  void *combined = malloc(80);
  if (combined != first)
    return 7;
  free(combined);

  /* realloc(NULL, n) is the allocator's ordinary fresh-allocation path. */
  void *from_null = realloc(0, 24);
  if (!from_null || ((unsigned long)from_null & 15UL) != 0)
    return 13;
  free(from_null);

  /* This implementation defines realloc(ptr, 0) as free followed by NULL. */
  void *zero_size = malloc(8);
  if (!zero_size || realloc(zero_size, 0) != 0)
    return 8;

  return 0;
}
