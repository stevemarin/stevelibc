#ifndef CTYPE_H
#define CTYPE_H

/*
 * Character classification and case conversion.
 *
 * This teaching implementation recognizes ASCII characters only and does not
 * depend on locale. Pass EOF or a value representable as unsigned char, as
 * required by the C interface; converting a possibly signed char first avoids
 * passing a negative value for bytes above 0x7f.
 */
int isalnum(int character);
int isalpha(int character);
int isblank(int character);
int iscntrl(int character);
int isdigit(int character);
int isgraph(int character);
int islower(int character);
int isprint(int character);
int ispunct(int character);
int isspace(int character);
int isupper(int character);
int isxdigit(int character);

/* Convert ASCII letters; every other input is returned unchanged. */
int tolower(int character);
int toupper(int character);

#endif
