#ifndef STDDEF_H
#define STDDEF_H

#include "./sys/types.h"

#ifndef NULL
#define NULL ((void *)0)
#endif

/* Ask the compiler for the true member offset, including target padding. */
#define offsetof(type, member) __builtin_offsetof(type, member)

/* An x86_64 type whose alignment can hold any scalar object. */
typedef union {
  long double __ld;
  void *__p;
  long long __ll;
} max_align_t;

#ifndef __cplusplus
typedef __WCHAR_TYPE__ wchar_t;
#endif

#endif
