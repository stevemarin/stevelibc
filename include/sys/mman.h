#ifndef SYS_MMAN_H
#define SYS_MMAN_H

#include "./types.h"

/* Memory access requested for pages in a mapping. */
#define PROT_NONE 0x0
#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define PROT_EXEC 0x4

/* Mapping sharing, placement, and backing-store behavior. */
#define MAP_SHARED 0x01
#define MAP_PRIVATE 0x02
#define MAP_FIXED 0x10
#define MAP_ANONYMOUS 0x20

/* mmap's failure sentinel is distinct from the valid mapping address zero. */
#define MAP_FAILED ((void *)-1)

/*
 * Map file-backed or anonymous pages into this process. The kernel chooses an
 * address when address is null; fd/offset identify file contents unless
 * MAP_ANONYMOUS is set. MAP_FAILED is returned on error.
 */
void *mmap(void *address, size_t length, int protection, int flags, int fd, off_t offset);
/* Remove a mapping or change its page protections; addresses/lengths are page-granular. */
int munmap(void *address, size_t length);
int mprotect(void *address, size_t length, int protection);

#endif
