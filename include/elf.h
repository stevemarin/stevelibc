#ifndef ELF_H
#define ELF_H

#include "./sys/types.h"

/*
 * The subset of the ELF64 ABI needed to inspect program headers and the
 * runtime linker's DT_DEBUG rendezvous. These layouts are x86_64 Linux ABI
 * structures: changing a field width or order would break binary parsing.
 */
typedef unsigned long Elf64_Addr;
typedef unsigned long Elf64_Off;
typedef unsigned short Elf64_Half;
typedef unsigned int Elf64_Word;
typedef long Elf64_Sword;
typedef unsigned long long Elf64_Xword;
typedef long long Elf64_Sxword;

#define EI_NIDENT 16
#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'
#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define ET_EXEC 2
#define ET_DYN 3

/* Program-header kinds describe mapped regions, not individual sections. */
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_PHDR 6

/* Dynamic tags identify values in an object's DT_* table. */
#define DT_NULL 0
#define DT_FINI 13
#define DT_DEBUG 21
#define DT_FINI_ARRAY 26
#define DT_FINI_ARRAYSZ 28

/*
 * ELF file header. It locates the program-header table used by the kernel and
 * dynamic loader; section-header fields are included for layout compatibility
 * even though this libc's runtime introspection does not need sections.
 */
typedef struct {
  unsigned char e_ident[EI_NIDENT]; /* Magic, class, byte order, and ABI version */
  Elf64_Half e_type;                /* ET_EXEC or ET_DYN */
  Elf64_Half e_machine;             /* Required processor architecture */
  Elf64_Word e_version;             /* ELF format version */
  Elf64_Addr e_entry;               /* Virtual entry address */
  Elf64_Off e_phoff;                /* File offset of program headers */
  Elf64_Off e_shoff;                /* File offset of section headers */
  Elf64_Word e_flags;               /* Architecture-specific flags */
  Elf64_Half e_ehsize;              /* Size of this header */
  Elf64_Half e_phentsize;           /* Size of each program-header record */
  Elf64_Half e_phnum;               /* Number of program-header records */
  Elf64_Half e_shentsize;           /* Size of each section-header record */
  Elf64_Half e_shnum;               /* Number of section-header records */
  Elf64_Half e_shstrndx;            /* Section-name string-table index */
} Elf64_Ehdr;

/*
 * A program header describes a segment the loader maps or interprets. p_offset
 * locates bytes in the file, while p_vaddr is their address in the image;
 * p_memsz can exceed p_filesz for zero-filled memory such as .bss.
 */
typedef struct {
  Elf64_Word p_type;    /* Segment role, for example PT_LOAD or PT_DYNAMIC */
  Elf64_Word p_flags;   /* Read/write/execute permissions */
  Elf64_Off p_offset;   /* Segment's file offset */
  Elf64_Addr p_vaddr;   /* Segment's virtual address before load bias */
  Elf64_Addr p_paddr;   /* Physical address; usually unused by user processes */
  Elf64_Xword p_filesz; /* Number of bytes present in the file */
  Elf64_Xword p_memsz;  /* Number of bytes in the mapped memory image */
  Elf64_Xword p_align;  /* Required segment alignment */
} Elf64_Phdr;

/*
 * One entry in an object's dynamic table. The tag determines whether the
 * union is an integer value (d_val) or an address (d_ptr); many addresses are
 * relative to the image and need the object's load bias before dereferencing.
 */
typedef struct {
  Elf64_Sxword d_tag; /* Meaning of the associated union value */
  union {
    Elf64_Xword d_val; /* Integer payload, such as an array byte size */
    Elf64_Addr d_ptr;  /* Address payload, such as a finalizer array */
  } d_un;
} Elf64_Dyn;

#endif
