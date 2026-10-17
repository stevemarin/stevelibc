#include "../include/sys/mman.h"
#include "../include/syscall.h"

/*
 * Linux x86_64 mmap takes six arguments; the syscall helper maps them to the
 * kernel ABI registers and translates a negative error result to MAP_FAILED.
 */
void *mmap(void *address, size_t length, int protection, int flags, int fd, off_t offset) {
  /* A successful mapping may be at address zero, so test specifically for -1. */
  long result = __syscall_result(syscall6(SYS_mmap, (long)address, (long)length, protection, flags, fd, offset));
  return result < 0 ? MAP_FAILED : (void *)result;
}

/* The kernel validates page alignment, range validity, and mapping ownership. */
int munmap(void *address, size_t length) {
  return (int)__syscall_result(syscall2(SYS_munmap, (long)address, (long)length));
}

int mprotect(void *address, size_t length, int protection) {
  return (int)__syscall_result(syscall3(SYS_mprotect, (long)address, (long)length, protection));
}
