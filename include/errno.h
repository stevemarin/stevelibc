#ifndef ERRNO_H
#define ERRNO_H

/* ==========================================================================
 * ERRNO.H : Error Numbers and Accessor Macro
 * ========================================================================== */

#define EPERM 1         /* Operation not permitted */
#define ENOENT 2        /* No such file or directory */
#define ESRCH 3         /* No such process */
#define EINTR 4         /* Interrupted system call */
#define E2BIG 7         /* Argument list too long */
#define ENOEXEC 8       /* Exec format error */
#define ECHILD 10       /* No child processes */
#define EIO 5           /* Input/output error */
#define EBADF 9         /* Bad file descriptor */
#define EAGAIN 11       /* Resource temporarily unavailable */
#define ENOMEM 12       /* Out of memory */
#define EACCES 13       /* Permission denied */
#define EFAULT 14       /* Bad address */
#define EBUSY 16        /* Device or resource busy */
#define EEXIST 17       /* File exists */
#define EXDEV 18        /* Invalid cross-device link */
#define ENODEV 19       /* No such device */
#define ENOTDIR 20      /* Not a directory */
#define EISDIR 21       /* Is a directory */
#define EINVAL 22       /* Invalid argument */
#define ENFILE 23       /* Too many open files in system */
#define EMFILE 24       /* Too many open files */
#define ENOTTY 25       /* Inappropriate ioctl for device */
#define ETXTBSY 26      /* Text file busy */
#define EFBIG 27        /* File too large */
#define ENOSPC 28       /* No space left on device */
#define ESPIPE 29       /* Illegal seek */
#define EROFS 30        /* Read-only file system */
#define EMLINK 31       /* Too many links */
#define EPIPE 32        /* Broken pipe */
#define EDOM 33         /* Numerical argument out of domain */
#define ERANGE 34       /* Numerical result out of range */
#define ENAMETOOLONG 36 /* Filename too long */
#define ENOSYS 38       /* Function not implemented */
#define ENOTEMPTY 39    /* Directory not empty */
#define ELOOP 40        /* Too many symbolic links */
#define EILSEQ 84       /* Invalid or incomplete multibyte sequence */
#define EOVERFLOW 75    /* Value too large for the defined data type */

/*
 * TLS accessor implemented by errno.c / architecture startup. Returning a
 * pointer (rather than exporting one global integer) gives each thread its
 * own error state while keeping errno usable as a C lvalue.
 */
int *__errno_location(void);

/*
 * errno is a macro, not a shared global integer: calling the accessor each
 * time selects the slot belonging to the current thread's TLS block.
 */
#define errno (*__errno_location())

#endif
