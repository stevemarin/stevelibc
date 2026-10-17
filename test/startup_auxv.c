#include "../include/sys/auxv.h"
#include "../include/unistd.h"

static int startup_sequence;

static void mark_preinitializer(void) { startup_sequence = 1; }

__attribute__((section(".preinit_array"), used)) static void (*const preinitializer)(void) = mark_preinitializer;

/* Ordinary constructors must run after preinitializers and before main. */
__attribute__((constructor)) static void mark_constructor_ran(void) {
  startup_sequence = startup_sequence == 1 ? 2 : -1;
}

/*
 * The kernel enters _start with argc, argv, envp, and auxv on the initial
 * stack. Check that startup publishes the environment and auxv to C code.
 */
int main(int argc, char **argv, char **envp) {
  if (startup_sequence != 2)
    return 5;

  if (argc < 1 || !argv || !argv[0])
    return 1;

  if (envp != environ || !__auxv_ptr)
    return 2;

  /* Linux must provide a nonzero, page-aligned base page size. */
  unsigned long page_size = getauxval(AT_PAGESZ);
  if (page_size < 4096 || (page_size & (page_size - 1)) != 0)
    return 3;

  /* An unrecognized tag is reported as zero by this minimal implementation. */
  if (getauxval(~0UL) != 0)
    return 4;

  return 0;
}
