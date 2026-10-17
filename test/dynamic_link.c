#include "../include/elf.h"
#include "../include/link.h"
#include "../include/stdlib.h"
#include "../include/sys/auxv.h"
#include "../include/unistd.h"

typedef struct {
  int objects;
  int main_image;
  int load_segment;
  int named_objects;
  int sample_library;
} image_report;

extern int stevelibc_dynamic_sample(void);
extern void stevelibc_dynamic_sample_set_finalizer_fd(int fd);

static int contains(const char *text, const char *part) {
  if (!text || !part || !*part)
    return 0;
  for (; *text; text++) {
    const char *left = text;
    const char *right = part;
    while (*left && *right && *left == *right) {
      left++;
      right++;
    }
    if (!*right)
      return 1;
  }
  return 0;
}

static int record_image(struct dl_phdr_info *info, size_t info_size, void *opaque) {
  image_report *report = (image_report *)opaque;
  if (info_size < sizeof(*info) || !info->dlpi_phdr || !info->dlpi_phnum)
    return -1;
  report->objects++;

  /* The main executable is the first image and has an empty loader name. */
  if (!info->dlpi_name || !*info->dlpi_name) {
    report->main_image++;
    for (Elf64_Half i = 0; i < info->dlpi_phnum; i++) {
      if (info->dlpi_phdr[i].p_type == PT_LOAD)
        report->load_segment++;
    }
  } else {
    report->named_objects++;
    if (contains(info->dlpi_name, "libstevelibc_sample.so"))
      report->sample_library++;
  }
  return 0;
}

static int stop_after_one(struct dl_phdr_info *info, size_t info_size, void *opaque) {
  int *calls = (int *)opaque;
  if (!info || info_size < sizeof(*info))
    return -1;
  (*calls)++;
  return 29;
}

/*
 * The kernel's ELF interpreter performs relocations before _start. This API
 * exposes the loaded program-header list for inspection; it is not dlopen.
 */
int main(void) {
  /* This imported call requires the interpreter to load the DSO and resolve
   * its dynamic symbol before the executable reaches main(). */
  if (stevelibc_dynamic_sample() != 0x5a17)
    return 1;

  unsigned long phdr = getauxval(AT_PHDR);
  unsigned long phnum = getauxval(AT_PHNUM);
  unsigned long phent = getauxval(AT_PHENT);
  if (!phdr || !phnum || phent != sizeof(Elf64_Phdr) || !getauxval(AT_BASE))
    return 1;

  image_report report = {0};
  if (dl_iterate_phdr(record_image, &report) != 0)
    return 2;
  /* This test is dynamically interpreted, so enumeration should include at
   * least the main image and the ELF interpreter (a named shared object). */
  if (report.objects < 2 || report.main_image != 1 || !report.load_segment || !report.named_objects ||
      report.sample_library != 1)
    return 3;

  int calls = 0;
  if (dl_iterate_phdr(stop_after_one, &calls) != 29 || calls != 1)
    return 4;
  if (dl_iterate_phdr(0, 0) != 0)
    return 5;

  /*
   * The child inherits the mapped DSO and tells its fini callback where to
   * write. exit() must invoke it; _exit() must deliberately bypass it.
   */
  int descriptors[2];
  if (pipe(descriptors) != 0)
    return 6;
  int child = fork();
  if (child < 0)
    return 7;
  if (child == 0) {
    close(descriptors[0]);
    stevelibc_dynamic_sample_set_finalizer_fd(descriptors[1]);
    exit(0);
  }
  close(descriptors[1]);
  int status = 0;
  if (waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
    return 8;
  char marker = 0;
  if (read(descriptors[0], &marker, 1) != 1 || marker != 'S')
    return 9;
  close(descriptors[0]);

  if (pipe(descriptors) != 0)
    return 10;
  child = fork();
  if (child < 0)
    return 11;
  if (child == 0) {
    close(descriptors[0]);
    stevelibc_dynamic_sample_set_finalizer_fd(descriptors[1]);
    _exit(0);
  }
  close(descriptors[1]);
  if (waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
    return 12;
  if (read(descriptors[0], &marker, 1) != 0 || close(descriptors[0]) != 0)
    return 13;

  return 0;
}
