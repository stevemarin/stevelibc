#ifndef SETJMP_H
#define SETJMP_H

/* ==========================================================================
 * SETJMP.H : Non-Local Jump Definitions
 * --------------------------------------------------------------------------
 * Provides support for saving CPU register context and jumping back across
 * function stack frames without executing normal return statements.
 * ========================================================================== */

/*
 * x86_64 jump context: callee-saved registers plus the stack and instruction
 * pointers needed to resume at the setjmp call site. This is an ABI-specific
 * machine context, not a portable serialized representation.
 */
typedef unsigned long jmp_buf[8];

/*
 * Save the current context. Returns zero initially and the value supplied to
 * longjmp when that context is restored (longjmp converts zero to one).
 */
int setjmp(jmp_buf env);

/* Restore a previously saved context; intervening stack frames are abandoned. */
void longjmp(jmp_buf env, int val);

#endif
