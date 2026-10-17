#include "../include/sys/auxv.h"

/* ==========================================================================
 * AUXV.C : Auxiliary Vector Query Implementation
 * --------------------------------------------------------------------------
 * Searches the parsed auxiliary vector table for a specific AT_* tag and
 * returns its associated value.
 * ========================================================================== */

extern Elf64_auxv_t *__auxv_ptr;

unsigned long getauxval(unsigned long type) {
  if (!__auxv_ptr)
    return 0;

  Elf64_auxv_t *aux = __auxv_ptr;
  while (aux->a_type != AT_NULL) {
    if (aux->a_type == type) {
      return aux->a_un.a_val;
    }
    aux++;
  }
  return 0; // Return 0 if requested auxiliary tag is not found
}
