#ifndef LINK_H
#define LINK_H

#include "./elf.h"

/*
 * Runtime description passed to dl_iterate_phdr callbacks. dlpi_phdr points
 * into a loaded ELF image and remains valid only while that object is loaded.
 * This project enumerates the process's current loaded objects; it does not
 * yet provide dlopen/dlsym/dlclose or loader-lock semantics.
 */
struct dl_phdr_info {
  Elf64_Addr dlpi_addr;
  const char *dlpi_name;
  const Elf64_Phdr *dlpi_phdr;
  Elf64_Half dlpi_phnum;
  unsigned long long dlpi_adds;
  unsigned long long dlpi_subs;
  size_t dlpi_tls_modid;
  void *dlpi_tls_data;
};

int dl_iterate_phdr(int (*callback)(struct dl_phdr_info *info, size_t size, void *data), void *data);

#endif
