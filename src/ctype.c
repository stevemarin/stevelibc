#include "../include/ctype.h"

/*
 * ASCII's letter and digit ranges are contiguous, so each predicate can be
 * expressed directly in terms of character codes without locale tables.
 */
int islower(int character) { return character >= 'a' && character <= 'z'; }

int isupper(int character) { return character >= 'A' && character <= 'Z'; }

int isalpha(int character) { return islower(character) || isupper(character); }

int isdigit(int character) { return character >= '0' && character <= '9'; }

int isalnum(int character) { return isalpha(character) || isdigit(character); }

/*
 * ASCII control bytes include DEL (0x7f). Space itself is printable, while
 * graph characters are the visible characters that are not whitespace.
 */
int iscntrl(int character) { return (character >= 0 && character < 0x20) || character == 0x7f; }

int isspace(int character) {
  return character == ' ' || (character >= '\t' && character <= '\r');
}

int isblank(int character) { return character == ' ' || character == '\t'; }

int isprint(int character) { return character >= ' ' && character <= '~'; }

int isgraph(int character) { return character >= '!' && character <= '~'; }

/*
 * Punctuation is visible ASCII that is neither a letter nor a decimal digit.
 * isxdigit includes decimal digits plus the six A-F/a-f letters.
 */
int ispunct(int character) { return isgraph(character) && !isalnum(character); }

int isxdigit(int character) {
  return isdigit(character) || (character >= 'a' && character <= 'f') ||
         (character >= 'A' && character <= 'F');
}

int tolower(int character) {
  if (isupper(character))
    return character + ('a' - 'A');
  return character;
}

int toupper(int character) {
  if (islower(character))
    return character - ('a' - 'A');
  return character;
}
