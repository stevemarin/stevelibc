#include "../include/elf.h"
#include "../include/link.h"
#include "../include/stddef.h"
#include "../include/sys/auxv.h"

/*
 * The ELF interpreter exposes its loaded-object list through the DT_DEBUG
 * entry in the main executable's dynamic section. These initial fields match
 * the stable x86_64 prefix of glibc's r_debug and link_map structures; they
 * are read-only here, not used to perform relocations or load new objects.
 */
typedef struct runtime_link_map runtime_link_map;
struct runtime_link_map {
  Elf64_Addr load_bias; /* Runtime base added to addresses in this ELF image */
  char *name; /* Loader-owned pathname, empty for the main executable */
  Elf64_Dyn *dynamic; /* DT_* table used to locate fini entries */
  runtime_link_map *next; /* Next object in loader traversal order */
  runtime_link_map *previous; /* Previous object; finalization walks backward */
};

typedef struct {
  int version; /* Runtime-linker rendezvous structure version */
  runtime_link_map *map; /* Head of the loaded-object list */
  Elf64_Addr breakpoint; /* Loader debug breakpoint address, unused here */
  int state; /* Loader activity state; only RT_CONSISTENT is safe to traverse */
  Elf64_Addr loader_base; /* Runtime linker's own load address */
} runtime_debug;

/* r_debug state value indicating that the link_map is stable for inspection. */
enum { RT_CONSISTENT = 0 };

static int valid_elf_header(const Elf64_Ehdr *header) {
  return header && header->e_ident[0] == ELFMAG0 && header->e_ident[1] == ELFMAG1 && header->e_ident[2] == ELFMAG2 &&
         header->e_ident[3] == ELFMAG3 && header->e_ident[4] == ELFCLASS64 && header->e_ident[5] == ELFDATA2LSB;
}

static int invoke_callback(int (*callback)(struct dl_phdr_info *, size_t, void *), void *data, Elf64_Addr load_bias,
                           const char *name, const Elf64_Phdr *phdr, Elf64_Half phnum) {
  struct dl_phdr_info info = {
      .dlpi_addr = load_bias,
      .dlpi_name = name ? name : "",
      .dlpi_phdr = phdr,
      .dlpi_phnum = phnum,
      /*
       * This compact ABI view does not track loader mutations or allocate
       * per-thread TLS blocks for foreign DSOs, so the extended counters and
       * TLS fields remain zero.
       */
      .dlpi_adds = 0,
      .dlpi_subs = 0,
      .dlpi_tls_modid = 0,
      .dlpi_tls_data = 0,
  };
  return callback(&info, sizeof(info), data);
}

/*
 * Use auxv as the authoritative description of the main executable. The
 * dynamic section is needed only to find DT_DEBUG, which the runtime loader
 * fills with its link-map rendezvous address after relocating the executable.
 */
static runtime_debug *find_runtime_debug(const Elf64_Phdr *phdr, Elf64_Half phnum, Elf64_Addr phdr_address,
                                         Elf64_Addr *load_bias) {
  int has_phdr_segment = 0;
  *load_bias = 0;
  for (Elf64_Half i = 0; i < phnum; i++) {
    if (phdr[i].p_type == PT_PHDR) {
      if (phdr_address < phdr[i].p_vaddr)
        return 0;
      *load_bias = phdr_address - phdr[i].p_vaddr;
      has_phdr_segment = 1;
      break;
    }
  }
  if (!has_phdr_segment)
    return 0;

  for (Elf64_Half i = 0; i < phnum; i++) {
    if (phdr[i].p_type != PT_DYNAMIC)
      continue;
    Elf64_Dyn *dynamic = (Elf64_Dyn *)(*load_bias + phdr[i].p_vaddr);
    unsigned long entry_count = phdr[i].p_memsz / sizeof(*dynamic);
    for (unsigned long j = 0; j < entry_count; j++) {
      if (dynamic[j].d_tag == DT_NULL)
        break;
      if (dynamic[j].d_tag == DT_DEBUG)
        return (runtime_debug *)dynamic[j].d_un.d_ptr;
    }
  }
  return 0;
}

/*
 * Normal process exit must also finalize dependency DSOs. The ELF interpreter
 * ran their constructors before _start, but this libc does not use the host
 * libc's __libc_start_main hook that normally schedules _dl_fini. Walk the
 * loader's link_map in reverse order and call each dependency's own fini
 * array/function. The main executable's fini array is handled by runtime.c.
 */
void __libc_run_dynamic_fini(void) {
  Elf64_Addr phdr_address = getauxval(AT_PHDR);
  unsigned long phnum_value = getauxval(AT_PHNUM);
  if (!phdr_address || !phnum_value || phnum_value > 0xffffUL || getauxval(AT_PHENT) != sizeof(Elf64_Phdr))
    return;

  const Elf64_Phdr *main_phdr = (const Elf64_Phdr *)phdr_address;
  Elf64_Half main_phnum = (Elf64_Half)phnum_value;
  Elf64_Addr main_bias = 0;
  runtime_debug *debug = find_runtime_debug(main_phdr, main_phnum, phdr_address, &main_bias);
  (void)main_bias;
  if (!debug || debug->state != RT_CONSISTENT || !debug->map)
    return;

  runtime_link_map *main_object = debug->map;
  runtime_link_map *object = main_object;
  while (object->next)
    object = object->next;

  for (; object && object != main_object; object = object->previous) {
    if (!object->dynamic)
      continue;
    Elf64_Addr fini_array_address = 0;
    Elf64_Xword fini_array_size = 0;
    Elf64_Addr fini_address = 0;

    /*
     * Dynamic pointers contain link-time virtual addresses in ET_DYN images;
     * add the object's load bias to reach their relocated runtime addresses.
     */
    for (Elf64_Dyn *entry = object->dynamic; entry->d_tag != DT_NULL; entry++) {
      if (entry->d_tag == DT_FINI_ARRAY)
        fini_array_address = object->load_bias + entry->d_un.d_ptr;
      else if (entry->d_tag == DT_FINI_ARRAYSZ)
        fini_array_size = entry->d_un.d_val;
      else if (entry->d_tag == DT_FINI)
        fini_address = object->load_bias + entry->d_un.d_ptr;
    }

    if (fini_array_address && fini_array_size >= sizeof(Elf64_Addr)) {
      void (**array)(void) = (void (**)(void))fini_array_address;
      unsigned long count = fini_array_size / sizeof(*array);
      while (count > 0) {
        void (*function)(void) = array[--count];
        if (function)
          function();
      }
    }
    if (fini_address)
      ((void (*)(void))fini_address)();
  }
}

/*
 * Recover program headers for an ET_DYN object from its mapped ELF header.
 * The main executable is handled separately because its ELF header may not be
 * present at load_bias (notably for non-PIE ET_EXEC binaries).
 */
static int mapped_object_headers(runtime_link_map *object, const Elf64_Phdr **phdr_out, Elf64_Half *phnum_out) {
  if (!object->load_bias)
    return 0;
  const Elf64_Ehdr *header = (const Elf64_Ehdr *)object->load_bias;
  if (!valid_elf_header(header) || header->e_type != ET_DYN || header->e_phentsize != sizeof(Elf64_Phdr) ||
      header->e_phnum == 0)
    return 0;
  *phdr_out = (const Elf64_Phdr *)(object->load_bias + header->e_phoff);
  *phnum_out = header->e_phnum;
  return 1;
}

/*
 * Enumerate currently mapped ELF images without taking the dynamic loader's
 * private lock. Callbacks should therefore not mutate the loader state, and a
 * concurrent external dlopen/dlclose is outside this minimal implementation's
 * guarantees. This provides introspection, not dynamic loading or symbol lookup.
 */
int dl_iterate_phdr(int (*callback)(struct dl_phdr_info *, size_t, void *), void *data) {
  if (!callback)
    return 0;

  Elf64_Addr phdr_address = getauxval(AT_PHDR);
  unsigned long phnum_value = getauxval(AT_PHNUM);
  unsigned long phent_value = getauxval(AT_PHENT);
  if (!phdr_address || !phnum_value || phnum_value > 0xffffUL || phent_value != sizeof(Elf64_Phdr))
    return 0;

  const Elf64_Phdr *main_phdr = (const Elf64_Phdr *)phdr_address;
  Elf64_Half main_phnum = (Elf64_Half)phnum_value;
  Elf64_Addr main_bias = 0;
  runtime_debug *debug = find_runtime_debug(main_phdr, main_phnum, phdr_address, &main_bias);

  if (!debug || !debug->map || debug->state != RT_CONSISTENT)
    return invoke_callback(callback, data, main_bias, "", main_phdr, main_phnum);

  runtime_link_map *main_object = debug->map;
  int result = invoke_callback(callback, data, main_bias, main_object->name, main_phdr, main_phnum);
  if (result)
    return result;

  /*
   * The first link_map node is the main program. Its headers came from auxv;
   * subsequent shared objects are ET_DYN mappings with ELF headers at base.
   */
  for (runtime_link_map *object = main_object->next; object; object = object->next) {
    const Elf64_Phdr *object_phdr;
    Elf64_Half object_phnum;
    if (!mapped_object_headers(object, &object_phdr, &object_phnum))
      continue;
    result = invoke_callback(callback, data, object->load_bias, object->name, object_phdr, object_phnum);
    if (result)
      return result;
  }
  return 0;
}
