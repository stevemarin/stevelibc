#include "../include/fcntl.h"
#include "../include/sys/auxv.h"
#include "../include/unistd.h"

/*
 * Coverage-mode executables use the host startup rather than the library's
 * assembly entry point. Read the real process auxv so startup and loader tests
 * still exercise actual kernel-provided values.
 */
static Elf64_auxv_t coverage_auxv[64];
Elf64_auxv_t *__auxv_ptr;

__attribute__((constructor(101))) static void load_coverage_auxv(void) {
  int descriptor = open("/proc/self/auxv", O_RDONLY);
  if (descriptor < 0)
    return;

  size_t bytes_read = 0;
  while (bytes_read < sizeof(coverage_auxv)) {
    ssize_t result = read(descriptor, (char *)coverage_auxv + bytes_read,
                          sizeof(coverage_auxv) - bytes_read);
    if (result <= 0)
      break;
    bytes_read += (size_t)result;
  }
  (void)close(descriptor);

  size_t count = bytes_read / sizeof(coverage_auxv[0]);
  if (count == 0)
    return;
  if (coverage_auxv[count - 1].a_type != AT_NULL && count < sizeof(coverage_auxv) / sizeof(coverage_auxv[0])) {
    coverage_auxv[count++] = (Elf64_auxv_t){.a_type = AT_NULL, .a_un.a_val = 0};
  }
  if (coverage_auxv[count - 1].a_type == AT_NULL)
    __auxv_ptr = coverage_auxv;
}
