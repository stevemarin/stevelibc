#ifndef STDARG_H
#define STDARG_H

/*
 * Variadic functions receive extra arguments through an implementation-defined
 * calling convention. The compiler knows that convention; these standard
 * macros expose its operations without requiring callers to use compiler names.
 */
typedef __builtin_va_list va_list;

/*
 * Initialize args after the final named parameter. va_arg must request each
 * value using its promoted type (for example, char is passed as int), in the
 * same order and with the same types the caller supplied.
 */
#define va_start(args, last_named_parameter) __builtin_va_start(args, last_named_parameter)
#define va_arg(args, type) __builtin_va_arg(args, type)

/* End a traversal before the va_list goes out of scope. */
#define va_end(args) __builtin_va_end(args)

/*
 * Make an independent traversal point. On x86_64 va_list may be an array-like
 * compiler type, so assignment is not a portable substitute for va_copy.
 */
#define va_copy(destination, source) __builtin_va_copy(destination, source)

#endif
