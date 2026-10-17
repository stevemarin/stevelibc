#ifndef STDLIB_H
#define STDLIB_H

/* ==========================================================================
 * STDLIB.H : Standard Utility, Memory Allocation, and Lifecycle Header
 * --------------------------------------------------------------------------
 * Declares heap management functions (`malloc`, `free`) and exit registration
 * hooks (`atexit`, `exit`).
 * ========================================================================== */

#include "./sys/types.h"

#define ATEXIT_MAX_FUNCS 128 // Fixed cleanup table capacity in runtime.c

/*
 * Integer quotient/remainder pairs. For valid nonzero divisors, quot is
 * truncated toward zero and rem satisfies numerator = quot * denominator
 * + rem. The remainder has the numerator's sign (or is zero).
 */
typedef struct {
  int quot;
  int rem;
} div_t;

typedef struct {
  long quot;
  long rem;
} ldiv_t;

typedef struct {
  long long quot;
  long long rem;
} lldiv_t;

/* Signature used for C cleanup hooks registered by atexit. */
typedef void (*void_func)(void);

/*
 * Register cleanup for normal exit. Callbacks run in reverse registration
 * order; _exit bypasses them. This implementation has a fixed table capacity.
 */
int atexit(void_func func);

/*
 * C++ ABI hooks: associate a destructor callback and its object argument with
 * the DSO that owns it. __cxa_finalize(handle) runs matching callbacks once;
 * a null handle means all outstanding callbacks.
 */
int __cxa_atexit(void (*func)(void *), void *argument, void *dso_handle);
void __cxa_finalize(void *dso_handle);
extern void *__dso_handle;

/*
 * Perform normal process cleanup (callbacks, ELF finalizers, stdio flush) and
 * terminate every thread. Unlike _exit, exit does not return to its caller.
 */
void exit(int status);

/*
 * Heap allocation backed by the process break. Returned storage is 16-byte
 * aligned; this educational allocator reuses and coalesces freed blocks but
 * does not return them to the kernel.
 */
void *malloc(size_t size);

/* Allocate nmemb * size bytes and initialize every byte to zero. */
void *calloc(size_t nmemb, size_t size);

/*
 * Resize a block while preserving its prior contents. A null ptr behaves like
 * malloc; size zero frees ptr and returns null in this implementation.
 */
void *realloc(void *ptr, size_t size);

/* Release a block allocated by this allocator; null and foreign pointers are ignored. */
void free(void *ptr);

/*
 * Search a sorted array using binary search. compare(key, element) must return
 * negative/zero/positive according to their ordering. The result points into
 * the array, or is null when no equal element exists; duplicate matches may
 * return any matching element. The array must be sorted by the same ordering.
 */
void *bsearch(const void *key, const void *base, size_t count, size_t size,
              int (*compare)(const void *, const void *));

/*
 * Return the magnitude of an integer in the matching signed type. As in C's
 * contract, the minimum signed value is not representable as a positive value
 * and is outside the defined input range.
 */
int abs(int value);
long labs(long value);
long long llabs(long long value);

/*
 * Divide using the corresponding integer type and return both quotient and
 * remainder. The divisor must not be zero, and the quotient must be
 * representable (notably, the minimum value divided by -1 is undefined).
 */
div_t div(int numerator, int denominator);
ldiv_t ldiv(long numerator, long denominator);
lldiv_t lldiv(long long numerator, long long denominator);

/*
 * Convert a signed or unsigned integer prefix in the requested base. A base
 * of zero detects decimal/octal/hex from the prefix. If end is non-null it
 * receives the first unconsumed character; overflow sets errno to ERANGE.
 */
long strtol(const char *string, char **end, int base);
unsigned long strtoul(const char *string, char **end, int base);
long long strtoll(const char *string, char **end, int base);
unsigned long long strtoull(const char *string, char **end, int base);
int atoi(const char *string);
long atol(const char *string);

/*
 * Environment strings are inherited at process startup. Mutating functions
 * may replace the environ vector and invalidate pointers previously returned
 * by getenv.
 */
char *getenv(const char *name);
/* Set name=value; overwrite==0 preserves an existing assignment. */
int setenv(const char *name, const char *value, int overwrite);
/* Remove every entry whose name matches; removing a missing name succeeds. */
int unsetenv(const char *name);
/* Remove all variables while retaining an empty mutable environment vector. */
int clearenv(void);

extern char **environ;

#endif
