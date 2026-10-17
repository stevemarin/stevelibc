#ifndef STRING_H
#define STRING_H

#include "./sys/types.h"

/*
 * Byte-wise memory operations. memcpy requires non-overlapping ranges;
 * memmove is the overlap-safe alternative.
 */
void *memcpy(void *destination, const void *source, size_t count);
void *memmove(void *destination, const void *source, size_t count);
void *memset(void *destination, int byte, size_t count);
int memcmp(const void *left, const void *right, size_t count);
void *memchr(const void *memory, int byte, size_t count);

/* Null-terminated byte string operations; strn* functions bound their work. */
size_t strlen(const char *string);
size_t strnlen(const char *string, size_t maximum);
int strcmp(const char *left, const char *right);
int strncmp(const char *left, const char *right, size_t count);
char *strcpy(char *destination, const char *source);
char *strncpy(char *destination, const char *source, size_t count);
char *strcat(char *destination, const char *source);
char *strncat(char *destination, const char *source, size_t count);
/*
 * Duplicate strings into malloc-owned storage. The caller must free the
 * returned pointer; strndup copies at most count bytes and always terminates.
 */
char *strdup(const char *string);
char *strndup(const char *string, size_t count);
char *strchr(const char *string, int byte);
char *strrchr(const char *string, int byte);
char *strstr(const char *string, const char *substring);
/* Count the initial span whose bytes are (or are not) in the accept set. */
size_t strspn(const char *string, const char *accept);
size_t strcspn(const char *string, const char *reject);
/* Find the first string byte present in accept; returns a pointer into string. */
char *strpbrk(const char *string, const char *accept);

/*
 * Return a static, read-only description for an errno value. The returned
 * storage belongs to the library and must not be modified or freed.
 */
char *strerror(int error_number);

#endif
