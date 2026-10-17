#include "../include/string.h"
#include "../include/errno.h"
#include "../include/stdlib.h"

/*
 * These routines are deliberately byte-oriented. The library is freestanding
 * and has no compiler runtime dependency for memory or string operations.
 */
void *memcpy(void *destination, const void *source, size_t count) {
  unsigned char *out = (unsigned char *)destination;
  const unsigned char *in = (const unsigned char *)source;
  /* The ranges must not overlap; callers needing overlap safety use memmove. */
  for (size_t i = 0; i < count; i++)
    out[i] = in[i];
  /* Return the original destination so calls can be used in expressions. */
  return destination;
}

void *memmove(void *destination, const void *source, size_t count) {
  unsigned char *out = (unsigned char *)destination;
  const unsigned char *in = (const unsigned char *)source;
  unsigned long out_address = (unsigned long)out;
  unsigned long in_address = (unsigned long)in;

  /*
   * If destination starts inside the source range, copying forward would
   * overwrite bytes that have not been read yet; copy from the end instead.
   */
  if (out_address > in_address && out_address - in_address < count) {
    while (count > 0) {
      count--;
      out[count] = in[count];
    }
  } else {
    /* Forward copying is safe when ranges are separate or destination is first. */
    for (size_t i = 0; i < count; i++)
      out[i] = in[i];
  }
  return destination;
}

void *memset(void *destination, int byte, size_t count) {
  unsigned char *out = (unsigned char *)destination;
  /* Only the low byte of the supplied int is repeated in each destination byte. */
  for (size_t i = 0; i < count; i++)
    out[i] = (unsigned char)byte;
  return destination;
}

int memcmp(const void *left, const void *right, size_t count) {
  const unsigned char *a = (const unsigned char *)left;
  const unsigned char *b = (const unsigned char *)right;
  for (size_t i = 0; i < count; i++) {
    /* Compare bytes as unsigned values, independent of the target char sign. */
    if (a[i] != b[i])
      return (int)a[i] - (int)b[i];
  }
  /* A zero result means the requested prefixes are byte-for-byte equal. */
  return 0;
}

void *memchr(const void *memory, int byte, size_t count) {
  const unsigned char *input = (const unsigned char *)memory;
  unsigned char wanted = (unsigned char)byte;
  for (size_t i = 0; i < count; i++) {
    if (input[i] == wanted)
      /* Return the first match as a pointer into the original memory region. */
      return (void *)(input + i);
  }
  /* No match was present in the bounded range. */
  return 0;
}

void *bsearch(const void *key, const void *base, size_t count, size_t size,
              int (*compare)(const void *, const void *)) {
  /*
   * Keep a half-open index range [first, last). Each comparison discards half
   * the remaining elements, so only O(log count) comparator calls are needed.
   */
  size_t first = 0;
  size_t last = count;
  const unsigned char *bytes = (const unsigned char *)base;

  while (first < last) {
    size_t middle = first + (last - first) / 2;
    const void *element = bytes + middle * size;
    int order = compare(key, element);
    if (order == 0)
      return (void *)element;
    if (order < 0)
      last = middle;
    else
      first = middle + 1;
  }
  return 0;
}

int abs(int value) { return value < 0 ? -value : value; }

long labs(long value) { return value < 0 ? -value : value; }

long long llabs(long long value) { return value < 0 ? -value : value; }

/*
 * C signed division truncates toward zero. Keeping the native / and %
 * operations together preserves the language's matching quotient/remainder
 * rule without needing a separate sign-adjustment algorithm.
 */
div_t div(int numerator, int denominator) {
  div_t result = {.quot = numerator / denominator, .rem = numerator % denominator};
  return result;
}

ldiv_t ldiv(long numerator, long denominator) {
  ldiv_t result = {.quot = numerator / denominator, .rem = numerator % denominator};
  return result;
}

lldiv_t lldiv(long long numerator, long long denominator) {
  lldiv_t result = {.quot = numerator / denominator, .rem = numerator % denominator};
  return result;
}

size_t strlen(const char *string) {
  const char *end = string;
  /* The terminator is excluded from the reported length. */
  while (*end)
    end++;
  return (size_t)(end - string);
}

size_t strnlen(const char *string, size_t maximum) {
  size_t length = 0;
  /* Stop at either the terminator or the caller's readable bound. */
  while (length < maximum && string[length])
    length++;
  return length;
}

int strcmp(const char *left, const char *right) {
  /* Advance while bytes match; the first differing byte determines ordering. */
  while (*left && (unsigned char)*left == (unsigned char)*right) {
    left++;
    right++;
  }
  return (int)(unsigned char)*left - (int)(unsigned char)*right;
}

int strncmp(const char *left, const char *right, size_t count) {
  /* Compare no more than count bytes, and stop early once both prefixes end. */
  for (size_t i = 0; i < count; i++) {
    unsigned char a = (unsigned char)left[i];
    unsigned char b = (unsigned char)right[i];
    if (a != b)
      return (int)a - (int)b;
    if (a == '\0')
      return 0;
  }
  return 0;
}

char *strcpy(char *destination, const char *source) {
  char *result = destination;
  /* Copy through and include the source terminator. */
  while ((*destination++ = *source++) != '\0') {
  }
  return result;
}

char *strncpy(char *destination, const char *source, size_t count) {
  size_t i = 0;
  while (i < count && source[i]) {
    destination[i] = source[i];
    i++;
  }
  /* strncpy pads the remaining field with NUL bytes when source is shorter. */
  while (i < count)
    destination[i++] = '\0';
  return destination;
}

char *strcat(char *destination, const char *source) {
  char *result = destination;
  /* Append at the existing terminator, then copy the source terminator too. */
  while (*destination)
    destination++;
  while ((*destination++ = *source++) != '\0') {
  }
  return result;
}

char *strncat(char *destination, const char *source, size_t count) {
  char *result = destination;
  /* count limits appended source bytes, not the final terminator. */
  while (*destination)
    destination++;
  size_t i = 0;
  while (i < count && source[i]) {
    destination[i] = source[i];
    i++;
  }
  destination[i] = '\0';
  return result;
}

char *strdup(const char *string) {
  size_t length = strlen(string);
  /* Include the terminator in the allocation; malloc owns this new copy. */
  if (length == ~0UL) {
    errno = ENOMEM;
    return 0;
  }
  char *copy = (char *)malloc(length + 1);
  if (!copy) {
    errno = ENOMEM;
    return 0;
  }
  memcpy(copy, string, length + 1);
  return copy;
}

char *strndup(const char *string, size_t count) {
  size_t length = strnlen(string, count);
  /* The copied prefix is always terminated, even if count cuts it short. */
  if (length == ~0UL) {
    errno = ENOMEM;
    return 0;
  }
  char *copy = (char *)malloc(length + 1);
  if (!copy) {
    errno = ENOMEM;
    return 0;
  }
  memcpy(copy, string, length);
  copy[length] = '\0';
  return copy;
}

char *strchr(const char *string, int byte) {
  unsigned char wanted = (unsigned char)byte;
  do {
    /* The terminating NUL is part of the searchable string. */
    if ((unsigned char)*string == wanted)
      return (char *)string;
  } while (*string++);
  return 0;
}

char *strrchr(const char *string, int byte) {
  unsigned char wanted = (unsigned char)byte;
  const char *match = 0;
  do {
    /* Keep updating the result so the final saved match is the last one. */
    if ((unsigned char)*string == wanted)
      match = string;
  } while (*string++);
  return (char *)match;
}

char *strstr(const char *string, const char *substring) {
  /* By convention, an empty needle matches at the beginning of the haystack. */
  if (!*substring)
    return (char *)string;
  for (const char *start = string; *start; start++) {
    const char *left = start;
    const char *right = substring;
    while (*left && *right && *left == *right) {
      left++;
      right++;
    }
    /* A match is complete only when every byte in the needle matched. */
    if (!*right)
      return (char *)start;
  }
  return 0;
}

size_t strspn(const char *string, const char *accept) {
  size_t length = 0;
  /* Advance while each byte in the initial run occurs in the accept set. */
  while (string[length] && strchr(accept, (unsigned char)string[length]))
    length++;
  return length;
}

size_t strcspn(const char *string, const char *reject) {
  size_t length = 0;
  /* Stop at the first byte that belongs to the reject set. */
  while (string[length] && !strchr(reject, (unsigned char)string[length]))
    length++;
  return length;
}

char *strpbrk(const char *string, const char *accept) {
  /* Unlike strspn, this searches anywhere and returns the matching byte. */
  while (*string) {
    if (strchr(accept, (unsigned char)*string))
      return (char *)string;
    string++;
  }
  return 0;
}
