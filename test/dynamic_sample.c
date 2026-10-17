/* Built as a tiny shared object to exercise the system interpreter's loader. */
static int finalizer_fd = -1;

int stevelibc_dynamic_sample(void) { return 0x5a17; }

void stevelibc_dynamic_sample_set_finalizer_fd(int fd) { finalizer_fd = fd; }

/*
 * No libc dependency is needed inside this DSO: issue write(2) directly so the
 * test can observe whether the custom runtime invokes dependency finalizers.
 */
__attribute__((destructor)) static void mark_dso_finalization(void) {
  if (finalizer_fd < 0)
    return;
  static const char marker = 'S';
  register long rax __asm__("rax") = 1;
  register long rdi __asm__("rdi") = finalizer_fd;
  register long rsi __asm__("rsi") = (long)&marker;
  register long rdx __asm__("rdx") = 1;
  __asm__ volatile("syscall" : "+a"(rax) : "D"(rdi), "S"(rsi), "d"(rdx) : "rcx", "r11", "memory");
}
