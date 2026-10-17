#include "../include/errno.h"
#include "../include/tls.h"

/* ==========================================================================
 * ERRNO.C : Error Location Accessor
 * --------------------------------------------------------------------------
 * Each thread's FS base points to a TLS control block whose first field is
 * its self-pointer and whose errno slot is at offset 8.
 * ========================================================================== */

tls_control_block __main_tcb = {.self = &__main_tcb};

_Static_assert(__builtin_offsetof(tls_control_block, errno_value) == 8, "errno TLS offset must match FS access");

int *__errno_location(void) {
  tls_control_block *tcb;
  __asm__ volatile("movq %%fs:0, %0" : "=r"(tcb));
  return &tcb->errno_value;
}
