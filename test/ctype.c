#include "../include/ctype.h"

/*
 * Classification functions are predicates: callers rely on zero meaning
 * false, while any nonzero result means true. Verify those meanings across
 * ASCII boundaries and check that unrelated values are left alone.
 */
int main(void) {
  if (isalnum('A') == 0 || isalnum('7') == 0 || isalnum('!') != 0)
    return 1;
  if (isalpha('A') == 0 || isalpha('z') == 0 || isalpha('[') != 0)
    return 2;
  if (isblank(' ') == 0 || isblank('\t') == 0 || isblank('\n') != 0)
    return 3;
  if (iscntrl('\0') == 0 || iscntrl('\n') == 0 || iscntrl(0x7f) == 0 ||
      iscntrl(' ') != 0)
    return 4;
  if (isdigit('0') == 0 || isdigit('9') == 0 || isdigit('/') != 0)
    return 5;
  if (isgraph('!') == 0 || isgraph('~') == 0 || isgraph(' ') != 0)
    return 6;
  if (islower('a') == 0 || islower('z') == 0 || islower('A') != 0)
    return 7;
  if (isprint(' ') == 0 || isprint('~') == 0 || isprint('\x7f') != 0)
    return 8;
  if (ispunct('!') == 0 || ispunct('_') == 0 || ispunct('A') != 0 ||
      ispunct('0') != 0 || ispunct(' ') != 0)
    return 9;
  if (isspace(' ') == 0 || isspace('\t') == 0 || isspace('\n') == 0 ||
      isspace('\v') == 0 || isspace('\f') == 0 || isspace('\r') == 0 ||
      isspace('\x1c') != 0)
    return 10;
  if (isupper('A') == 0 || isupper('Z') == 0 || isupper('a') != 0)
    return 11;
  if (isxdigit('0') == 0 || isxdigit('9') == 0 || isxdigit('a') == 0 ||
      isxdigit('F') == 0 || isxdigit('g') != 0 || isxdigit('G') != 0)
    return 12;

  /* Case conversion changes ASCII letters only, including idempotent cases. */
  if (tolower('A') != 'a' || tolower('Z') != 'z' || tolower('m') != 'm' ||
      tolower('7') != '7' || tolower(-1) != -1)
    return 13;
  if (toupper('a') != 'A' || toupper('z') != 'Z' || toupper('M') != 'M' ||
      toupper('7') != '7' || toupper(-1) != -1)
    return 14;

  /*
   * The implementation is deliberately ASCII-only: all high-bit byte values,
   * when represented as unsigned char, are non-classified and unchanged.
   */
  for (int byte = 0x80; byte <= 0xff; byte++) {
    if (isalnum(byte) || isalpha(byte) || isblank(byte) || iscntrl(byte) ||
        isdigit(byte) || isgraph(byte) || islower(byte) || isprint(byte) ||
        ispunct(byte) || isspace(byte) || isupper(byte) || isxdigit(byte) ||
        tolower(byte) != byte || toupper(byte) != byte)
      return 15;
  }

  /* EOF is accepted and is not classified or transformed. */
  if (isalnum(-1) || isalpha(-1) || isblank(-1) || iscntrl(-1) || isdigit(-1) ||
      isgraph(-1) || islower(-1) || isprint(-1) || ispunct(-1) || isspace(-1) ||
      isupper(-1) || isxdigit(-1) || tolower(-1) != -1 || toupper(-1) != -1)
    return 16;

  /* Exhaustive ASCII checks ensure the predicates agree on their categories. */
  for (int character = 0; character <= 0x7f; character++) {
    if (!!isalnum(character) != !!(isalpha(character) || isdigit(character)))
      return 17;
    if (!!isgraph(character) != !!(isprint(character) && !isspace(character)))
      return 18;
    if (!!ispunct(character) != !!(isgraph(character) && !isalnum(character)))
      return 19;
    if (!!isxdigit(character) !=
        !!(isdigit(character) || (character >= 'a' && character <= 'f') ||
           (character >= 'A' && character <= 'F')))
      return 20;
  }

  return 0;
}
