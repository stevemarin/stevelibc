#ifndef SYS_AUXV_H
#define SYS_AUXV_H

/* ==========================================================================
 * SYS/AUXV.H : Auxiliary Vector (auxv) Definitions and Accessors
 * --------------------------------------------------------------------------
 * The auxiliary vector is passed by the Linux kernel on startup, providing
 * vital system information (e.g., page size, CPU capability flags, random seed).
 * ========================================================================== */

/* --- Auxiliary Vector Type Identifiers --- */
#define AT_NULL 0      // End of auxiliary vector
#define AT_IGNORE 1    // Entry should be ignored
#define AT_EXECFD 2    // File descriptor of program
#define AT_PHDR 3      // Program header address of executable
#define AT_PHENT 4     // Size of program header entry
#define AT_PHNUM 5     // Number of program headers
#define AT_PAGESZ 6    // System page size (typically 4096 bytes)
#define AT_BASE 7      // Base address of interpreter (linker)
#define AT_FLAGS 8     // Flags
#define AT_ENTRY 9     // Entry point of program
#define AT_NOTELF 10   // Program is not ELF
#define AT_UID 11      // Real user ID
#define AT_EUID 12     // Effective user ID
#define AT_GID 13      // Real group ID
#define AT_EGID 14     // Effective group ID
#define AT_PLATFORM 15 // String identifying CPU platform
#define AT_HWCAP 16    // Hardware capability bitmask
#define AT_CLKTCK 17   // Frequency of times()
#define AT_SECURE 23   // Secure mode boolean (suid/sgid binaries)
#define AT_RANDOM 25   // Address of 16 random bytes
#define AT_HWCAP2 26   // Extension of AT_HWCAP
#define AT_EXECFN 31   // Pathname used to execute the program

/*
 * One kernel-provided auxiliary-vector record. The vector follows envp on the
 * initial process stack and ends with AT_NULL. Linux x86_64 exposes the
 * payload through the a_un union; a_val is the integer view used by getauxval.
 */
typedef struct {
  unsigned long a_type; // Entry type identifier (AT_*)
  union {
    unsigned long a_val; // Integer value associated with type
  } a_un;
} Elf64_auxv_t;

/*
 * Return the payload for type, or zero if that tag is absent. Zero is also a
 * valid payload for some tags, so this subset does not distinguish absence.
 */
unsigned long getauxval(unsigned long type);

#endif
