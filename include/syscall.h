#ifndef SYSCALL_H
#define SYSCALL_H

#include "./errno.h"

/* ==========================================================================
 * SYSCALL.H : Low-Level Linux System Call Dispatcher & Register Interface
 * --------------------------------------------------------------------------
 * In a freestanding C environment without glibc, your code has no standard
 * library stubs. To talk to the Linux kernel, we must manually place system
 * call numbers into %rax, arguments into designated registers (%rdi, %rsi, etc.),
 * and execute the 'syscall' instruction.
 * ========================================================================== */

/* --- Linux x86_64 System Call Identifiers --- */
#define SYS_read 0              // Read from a file descriptor
#define SYS_write 1             // Write to a file descriptor
#define SYS_open 2              // Open a file path
#define SYS_close 3             // Close a file descriptor
#define SYS_fstat 5             // Query metadata for a file descriptor
#define SYS_fsync 74            // Flush file data and metadata to storage
#define SYS_truncate 76         // Change a file's length by pathname
#define SYS_ftruncate 77        // Change a file's length through its descriptor
#define SYS_chdir 80            // Change the process working directory
#define SYS_fchdir 81           // Change the process working directory by descriptor
#define SYS_mkdir 83            // Create a filesystem directory
#define SYS_rmdir 84            // Remove an empty filesystem directory
#define SYS_rename 82           // Rename a filesystem name
#define SYS_link 86             // Create a hard link
#define SYS_symlink 88          // Create a symbolic link
#define SYS_readlink 89         // Read a symbolic link's stored target
#define SYS_chmod 90            // Change file permission bits by path
#define SYS_fchmod 91           // Change file permission bits by descriptor
#define SYS_umask 95            // Set the process file-creation mask
#define SYS_getpid 39           // Get the calling process ID
#define SYS_gettid 186          // Get the calling thread ID
#define SYS_getppid 110         // Get the parent process ID
#define SYS_setpgid 109         // Set a process's process group
#define SYS_setsid 112          // Create a new session and process group
#define SYS_getpgid 121         // Get a process's process group ID
#define SYS_getsid 124          // Get a process's session ID
#define SYS_getuid 102          // Get the real user ID
#define SYS_geteuid 107         // Get the effective user ID
#define SYS_getgid 104          // Get the real group ID
#define SYS_getegid 108         // Get the effective group ID
#define SYS_access 21           // Check file access permissions
#define SYS_kill 62             // Send a signal to a process or process group
#define SYS_dup 32              // Duplicate a file descriptor
#define SYS_dup2 33             // Duplicate a file descriptor to a chosen number
#define SYS_pipe 22             // Create a pipe
#define SYS_unlink 87           // Remove a filesystem name
#define SYS_ioctl 16            // Device control / TTY configuration
#define SYS_rt_sigaction 13     // Register signal interception handlers
#define SYS_rt_sigprocmask 14   // Block/unblock signal delivery masks
#define SYS_rt_sigreturn 15     // Return from user-space signal handlers
#define SYS_rt_sigpending 127   // Read the calling thread's pending signal set
#define SYS_rt_sigtimedwait 128 // Synchronously accept a blocked signal
#define SYS_rt_sigsuspend 130   // Temporarily replace the calling thread's mask
#define SYS_sigaltstack 131     // Configure an alternate signal-handler stack
#define SYS_tgkill 234          // Send a signal to a specific thread in a process
#define SYS_brk 12              // Adjust the program data segment (heap) break
#define SYS_lseek 8             // Reposition the file offset
#define SYS_poll 7              // Wait for readiness on a set of file descriptors
#define SYS_mmap 9              // Map files or anonymous memory
#define SYS_mprotect 10         // Change memory protection
#define SYS_munmap 11           // Unmap memory
#define SYS_wait4 61            // Wait for a child process
#define SYS_fcntl 72            // File descriptor control
#define SYS_getcwd 79           // Get the current working directory
#define SYS_openat 257          // Open relative to a directory descriptor
#define SYS_mkdirat 258         // Create a directory relative to a directory descriptor
#define SYS_newfstatat 262      // Query metadata for a path
#define SYS_unlinkat 263        // Remove a path relative to a directory descriptor
#define SYS_renameat 264        // Rename paths relative to directory descriptors
#define SYS_linkat 265          // Create a hard link relative to directory descriptors
#define SYS_symlinkat 266       // Create a symbolic link relative to a directory descriptor
#define SYS_readlinkat 267      // Read a symbolic link relative to a directory descriptor
#define SYS_faccessat 269       // Check access relative to a directory descriptor
#define SYS_getdents64 217      // Read directory entries
#define SYS_fork 57             // Duplicate current process
#define SYS_execve 59           // Execute a new program binary
#define SYS_exit 60             // Terminate current process immediately
#define SYS_exit_group 231      // Terminate all threads in the process
#define SYS_gettimeofday 96     // Retrieve wall-clock time
#define SYS_nanosleep 35        // Suspend the calling thread for a relative interval
#define SYS_arch_prctl 158      // Set architecture-specific thread states (TLS)
#define SYS_futex 202           // Fast userspace mutex sleeping/awakening
#define SYS_clock_gettime 228   // High-resolution monotonic timing
#define SYS_epoll_wait 232      // Wait for epoll event notifications
#define SYS_epoll_ctl 233       // Control interest list for epoll file descriptors
#define SYS_epoll_create1 291   // Create an epoll event notification instance
#define SYS_time 201            // Get time in seconds

/*
 * Convert a Linux raw syscall result to libc-style error handling.
 * Linux reserves -1 through -4095 for negative errno values; other results,
 * including successful negative values outside that range, pass through.
 */
static inline long __syscall_result(long result) {
  if (result < 0 && result >= -4095) {
    errno = (int)-result;
    return -1;
  }
  return result;
}

/*
 * 0-Argument System Call Wrapper
 * n: System call number placed into %rax.
 * Returns the kernel's return value from %rax.
 */
static inline long syscall0(long n) {
  long ret;
  __asm__ volatile("syscall"                // Trigger kernel mode transition
                   : "=a"(ret)              // Output: capture %rax into variable 'ret'
                   : "0"(n)                 // Input: constraint "0" ensures 'n' is placed in %rax before call
                   : "rcx", "r11", "memory" // Clobbers: kernel trashes %rcx and %r11; "memory" prevents caching
  );
  return ret;
}

/*
 * 1-Argument System Call Wrapper
 * a1: First argument placed into register %rdi via constraint "D".
 */
static inline long syscall1(long n, long a1) {
  long ret;
  __asm__ volatile("syscall" : "=a"(ret) : "0"(n), "D"(a1) : "rcx", "r11", "memory");
  return ret;
}

/*
 * 2-Argument System Call Wrapper
 * a1 -> %rdi ("D"), a2 -> %rsi ("S")
 */
static inline long syscall2(long n, long a1, long a2) {
  long ret;
  __asm__ volatile("syscall" : "=a"(ret) : "0"(n), "D"(a1), "S"(a2) : "rcx", "r11", "memory");
  return ret;
}

/*
 * 3-Argument System Call Wrapper
 * a1 -> %rdi ("D"), a2 -> %rsi ("S"), a3 -> %rdx ("d")
 */
static inline long syscall3(long n, long a1, long a2, long a3) {
  long ret;
  __asm__ volatile("syscall" : "=a"(ret) : "0"(n), "D"(a1), "S"(a2), "d"(a3) : "rcx", "r11", "memory");
  return ret;
}

/*
 * 4-Argument System Call Wrapper
 * Linux x86_64 ABI requires the 4th argument in register %r10 (not %rcx).
 */
static inline long syscall4(long n, long a1, long a2, long a3, long a4) {
  long ret;
  register long r10 __asm__("r10") = a4; // Force argument 4 into %r10
  __asm__ volatile("syscall" : "=a"(ret) : "0"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10) : "rcx", "r11", "memory");
  return ret;
}

/*
 * Five arguments use the same Linux x86_64 register assignment as syscall6;
 * r9 remains unused. linkat is an example of an operation with five args.
 */
static inline long syscall5(long n, long a1, long a2, long a3, long a4, long a5) {
  long ret;
  register long r10 __asm__("r10") = a4;
  register long r8 __asm__("r8") = a5;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "0"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8)
                   : "rcx", "r11", "memory");
  return ret;
}

/*
 * 6-Argument System Call Wrapper
 * Maps args 1 through 6 across %rdi, %rsi, %rdx, %r10, %r8, and %r9.
 */
static inline long syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6) {
  long ret;
  register long r10 __asm__("r10") = a4;
  register long r8 __asm__("r8") = a5;
  register long r9 __asm__("r9") = a6;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "0"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9)
                   : "rcx", "r11", "memory");
  return ret;
}

#endif
