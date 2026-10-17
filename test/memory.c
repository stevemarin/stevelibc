#include "../include/string.h"
#include "../include/stdlib.h"

/*
 * Cover byte ordering, overlap handling, null terminators, padding, and
 * unsigned-byte comparison so these routines work for arbitrary byte data.
 */
int main(void) {
  /* Zero-length operations must not inspect or modify the pointed-to bytes. */
  unsigned char zero_length = 0x5a;
  char empty_needle_haystack[] = "abc";
  if (memcpy(&zero_length, &zero_length, 0) != &zero_length || zero_length != 0x5a ||
      memmove(&zero_length, &zero_length, 0) != &zero_length || memset(&zero_length, 0, 0) != &zero_length ||
      memcmp(&zero_length, &zero_length, 0) != 0 || memchr(&zero_length, 0x5a, 0) != 0)
    return 12;
  if (strnlen("", 0) != 0 || strncmp("", "", 4) != 0 ||
      strstr(empty_needle_haystack, "") != empty_needle_haystack)
    return 13;

  unsigned char source[] = {0x00, 0x7f, 0x80, 0xff};
  unsigned char copy[4];
  /* Check the returned destination and equality over the full byte range. */
  if (memcpy(copy, source, sizeof(source)) != copy || memcmp(copy, source, sizeof(source)) != 0)
    return 1;
  if (memcmp(source, (unsigned char[]){0x00, 0x7f, 0x7f, 0xff}, sizeof(source)) <= 0)
    return 2;
  if (memcmp(source, (unsigned char[]){0x00, 0x7f, 0x80, 0xfe}, sizeof(source)) <= 0)
    return 14;
  if (memchr(source, 0x80, sizeof(source)) != &source[2] || memchr(source, 1, sizeof(source)))
    return 3;

  char overlap[] = "abcdef";
  /* Test both backward and forward overlap directions. */
  memmove(overlap + 1, overlap, 5);
  if (strcmp(overlap, "aabcde") != 0)
    return 4;
  memmove(overlap, overlap + 1, 5);
  if (strcmp(overlap, "abcdee") != 0)
    return 5;

  char text[32];
  /* Starting from a filled buffer makes strncpy's zero padding observable. */
  memset(text, 'x', sizeof(text));
  strcpy(text, "mini");
  if (strlen(text) != 4 || strnlen(text, 2) != 2)
    return 6;
  if (strcmp(text, "mini") != 0 || strncmp(text, "minx", 3) != 0)
    return 7;
  strncpy(text + 5, "ab", 5);
  if (text[5] != 'a' || text[6] != 'b' || text[7] != '\0' || text[9] != '\0')
    return 8;
  strcat(text, "!");
  strncat(text, "more", 2);
  if (strcmp(text, "mini!mo") != 0)
    return 9;
  if (strchr(text, '!') != text + 4 || strrchr(text, 'm') != text + 5)
    return 10;
  if (strchr(text, '\0') != text + strlen(text) || strstr(text, "i!m") != text + 3 || strstr(text, "missing"))
    return 11;
  if (strchr(text, 'z') || strrchr(text, 'z') || strrchr(text, '\0') != text + strlen(text))
    return 15;

  /* Duplicated strings have independent, malloc-owned storage. */
  char *duplicate = strdup("copy me");
  if (!duplicate || strcmp(duplicate, "copy me") != 0)
    return 16;
  duplicate[0] = 'C';
  if (duplicate[0] != 'C')
    return 17;
  free(duplicate);

  char *bounded_duplicate = strndup("bounded", 4);
  if (!bounded_duplicate || strcmp(bounded_duplicate, "boun") != 0)
    return 18;
  free(bounded_duplicate);
  bounded_duplicate = strndup("short", 20);
  if (!bounded_duplicate || strcmp(bounded_duplicate, "short") != 0)
    return 19;
  free(bounded_duplicate);
  bounded_duplicate = strndup("empty", 0);
  if (!bounded_duplicate || bounded_duplicate[0] != '\0')
    return 20;
  free(bounded_duplicate);

  /* Span routines distinguish the initial run from later matches. */
  if (strspn("abcde123", "abcde") != 5 || strspn("abc", "") != 0 ||
      strspn("", "abc") != 0)
    return 21;
  if (strcspn("abcde123", "123") != 5 || strcspn("abc", "") != 3 ||
      strcspn("", "abc") != 0)
    return 22;
  char searchable[] = "hello world";
  if (strpbrk(searchable, "ow") != searchable + 4 ||
      strpbrk("hello", "") != 0 || strpbrk("hello", "xyz") != 0)
    return 23;

  /* Membership checks operate on unsigned byte values, even above ASCII. */
  char high_byte_string[] = {(char)0xff, 'x', '\0'};
  char high_byte_set[] = {(char)0xff, '\0'};
  if (strspn(high_byte_string, high_byte_set) != 1 ||
      strcspn(high_byte_string, high_byte_set) != 0 ||
      strpbrk(high_byte_string + 1, high_byte_set) != 0)
    return 24;

  return 0;
}
