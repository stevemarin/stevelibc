#ifndef UNISTD_H
#define UNISTD_H

#include "./fcntl.h"
#include "./sys/auxv.h"
#include "./sys/types.h"
#include "./sys/wait.h"

/* access/faccessat mode bits; F_OK asks only whether the path exists. */
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

/* NULL-terminated name=value vector inherited from the initial process stack. */
extern char **environ;

/* Pointer to the raw AT_* records that follow envp on the initial stack. */
extern Elf64_auxv_t *__auxv_ptr;

/*
 * Descriptor I/O. read/write may transfer fewer than count bytes; callers that
 * require a complete transfer must loop. Descriptors are small kernel-managed
 * integers referring to open-file descriptions, not FILE streams.
 */
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
/* open/openat return a new descriptor; pass mode when O_CREAT is set. */
int open(const char *path, int flags, ...);
int close(int fd);
/* lseek changes the shared open-file offset; ESPIPE is typical for pipes. */
off_t lseek(int fd, off_t offset, int whence);
/* Duplicates refer to the same open-file description and therefore share offset. */
int dup(int fd);
int dup2(int oldfd, int newfd);
/* Create a unidirectional byte stream; pipefd[0] reads and pipefd[1] writes. */
int pipe(int pipefd[2]);

/*
 * Path and filesystem operations. *at variants resolve relative names from a
 * directory descriptor (or AT_FDCWD), reducing dependence on current cwd.
 */
int access(const char *path, int mode);
int faccessat(int dirfd, const char *path, int mode);
int unlink(const char *path);
int unlinkat(int dirfd, const char *path, int flags);
int mkdir(const char *path, unsigned int mode);
int mkdirat(int dirfd, const char *path, mode_t mode);
int rmdir(const char *path);
int rename(const char *oldpath, const char *newpath);
int renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath);
int link(const char *oldpath, const char *newpath);
int linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags);
int symlink(const char *target, const char *linkpath);
int symlinkat(const char *target, int newdirfd, const char *linkpath);
/* readlink copies link text without appending NUL; result is copied byte count. */
ssize_t readlink(const char *path, char *buffer, size_t size);
ssize_t readlinkat(int dirfd, const char *path, char *buffer, size_t size);
int chmod(const char *path, mode_t mode);
int fchmod(int fd, mode_t mode);
int ftruncate(int fd, off_t length);
int truncate(const char *path, off_t length);
/* Request that pending file data and metadata be committed to storage. */
int fsync(int fd);
/* Set process-wide creation mask, returning its previous value. */
mode_t umask(mode_t mask);
int chdir(const char *path);
int fchdir(int fd);

/*
 * Process and credential identifiers are supplied by the kernel. These
 * queries cannot fail for the current process/thread in ordinary use.
 */
pid_t getpid(void);
pid_t getppid(void);
uid_t getuid(void);
uid_t geteuid(void);
gid_t getgid(void);
gid_t getegid(void);

/*
 * fork creates a child process; zero is returned in the child and its PID in
 * the parent. wait/waitpid reap child state and optionally return encoded
 * status, interpreted with the WIFEXITED, WEXITSTATUS, and related macros.
 */
int fork(void);
pid_t wait(int *status);
pid_t waitpid(pid_t pid, int *status, int options);
/* Send a signal; pid may name a process, process group, or special target. */
int kill(pid_t pid, int signal);
int setpgid(pid_t pid, pid_t pgid);
pid_t getpgid(pid_t pid);
pid_t setsid(void);
pid_t getsid(pid_t pid);
/* Return the current directory into caller storage; null indicates failure. */
char *getcwd(char *buffer, size_t size);

/*
 * Replace the current process image. Success never returns; failure returns
 * -1 and leaves the caller running. execvp searches PATH for a basename.
 */
int execve(const char *filename, char *const argv[], char *const envp[]);
int execv(const char *filename, char *const argv[]);
int execvp(const char *filename, char *const argv[]);
/* Terminate immediately without running exit handlers or flushing stdio. */
void _exit(int status);

#endif
