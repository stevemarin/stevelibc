#include "../include/dirent.h"
#include "../include/errno.h"
#include "../include/fcntl.h"
#include "../include/limits.h"
#include "../include/stddef.h"
#include "../include/stdint.h"
#include "../include/stdlib.h"
#include "../include/sys/stat.h"
#include "../include/sys/wait.h"
#include "../include/unistd.h"

static int equal(const char *left, const char *right) {
  while (*left && *left == *right) {
    left++;
    right++;
  }
  return *left == *right;
}

static int append(char *destination, const char *source, int at) {
  while (*source)
    destination[at++] = *source++;
  destination[at] = '\0';
  return at;
}

int main(void) {
  /* Public foundational headers expose expected x86_64 widths and layout tools. */
  if (sizeof(size_t) != 8 || sizeof(uint64_t) != 8 || sizeof(struct stat) != 144 ||
      offsetof(struct stat, st_mode) != 24 || offsetof(struct stat, st_size) != 48)
    return 1;

  /* Environment accessors validate names before searching or mutating storage. */
  errno = 0;
  if (getenv(0) || getenv("") || getenv("PATH=extra") || errno != 0)
    return 30;
  errno = 0;
  if (setenv(0, "x", 1) != -1 || errno != EINVAL)
    return 31;
  errno = 0;
  if (setenv("", "x", 1) != -1 || errno != EINVAL)
    return 32;
  errno = 0;
  if (setenv("MISSING_VALUE", 0, 1) != -1 || errno != EINVAL)
    return 33;
  errno = 0;
  if (unsetenv(0) != -1 || errno != EINVAL)
    return 34;
  errno = 0;
  if (unsetenv("") != -1 || errno != EINVAL)
    return 35;
  errno = 0;
  if (unsetenv("BAD=NAME") != -1 || errno != EINVAL)
    return 36;

  /* Numeric conversions cover auto-detected bases, end pointers, and overflow. */
  char *end = 0;
  if (strtol(" -0x2a!", &end, 0) != -42 || !end || *end != '!')
    return 2;
  if (strtol("+", &end, 10) != 0 || end == 0 || *end != '+')
    return 44;
  if (strtol("0X2A", &end, 0) != 42 || !end || *end != '\0')
    return 45;
  if (strtoll("-9223372036854775808", 0, 10) != LLONG_MIN)
    return 27;
  if (strtoul("-1", 0, 10) != UINT64_MAX)
    return 3;
  errno = 0;
  if (strtol("999999999999999999999999", 0, 10) != LONG_MAX || errno != ERANGE)
    return 4;
  errno = 0;
  if (strtol("12", 0, 1) != 0 || errno != EINVAL)
    return 5;
  if (atoi("-123") != -123 || atol("456") != 456)
    return 6;

  /* Mutations copy the startup environment before changing its stack-backed array. */
  const char *startup_path = getenv("PATH");
  if (!startup_path || setenv("STEVELIBC_TEST_ENV", "first", 1) != 0)
    return 7;
  if (!equal(getenv("STEVELIBC_TEST_ENV"), "first"))
    return 8;
  if (setenv("STEVELIBC_TEST_ENV", "ignored", 0) != 0 || !equal(getenv("STEVELIBC_TEST_ENV"), "first"))
    return 9;
  if (setenv("STEVELIBC_TEST_ENV", "second", 1) != 0 || !equal(getenv("STEVELIBC_TEST_ENV"), "second"))
    return 10;
  if (unsetenv("STEVELIBC_TEST_ENV") != 0 || getenv("STEVELIBC_TEST_ENV"))
    return 11;
  if (setenv("BAD=NAME", "x", 1) != -1 || errno != EINVAL)
    return 12;
  if (!getenv("PATH") || !equal(getenv("PATH"), startup_path))
    return 13;
  if (unsetenv("NOT_PRESENT_STEVELIBC_TEST") != 0)
    return 37;

  /*
   * Add enough distinct, process-specific names to grow the owned environ
   * vector; then remove them to leave only the test's own environment edits.
   */
  char growth_name[] = "STEVELIBC_COV_0000000000_000";
  unsigned long growth_pid = (unsigned long)getpid();
  char reversed_pid[20];
  int pid_digits = 0;
  do {
    reversed_pid[pid_digits++] = (char)('0' + growth_pid % 10);
    growth_pid /= 10;
  } while (growth_pid);
  int name_at = 14;
  while (pid_digits)
    growth_name[name_at++] = reversed_pid[--pid_digits];
  growth_name[name_at] = '_';
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 10; j++) {
      growth_name[name_at + 1] = (char)('0' + i);
      growth_name[name_at + 2] = (char)('0' + j);
      if (setenv(growth_name, "x", 1) != 0)
        return 38;
    }
  }
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 10; j++) {
      growth_name[name_at + 1] = (char)('0' + i);
      growth_name[name_at + 2] = (char)('0' + j);
      if (!getenv(growth_name) || unsetenv(growth_name) != 0)
        return 39;
    }
  }
  if (clearenv() != 0 || getenv("PATH") || setenv("AFTER_CLEAR", "ok", 1) != 0 || !equal(getenv("AFTER_CLEAR"), "ok"))
    return 28;

  errno = 0;
  if (opendir("/path/that/does/not/exist/stevelibc") || errno != ENOENT)
    return 40;
  errno = 0;
  if (readdir(0) || errno != EBADF)
    return 41;
  errno = 0;
  if (dirfd(0) != -1 || errno != EBADF)
    return 42;
  errno = 0;
  if (closedir(0) != -1 || errno != EBADF)
    return 43;

  /* Create a private directory and exercise metadata and relative openat. */
  char directory[80] = "/tmp/stevelibc-api-";
  unsigned long pid = (unsigned long)getpid();
  char reversed[20];
  int digits = 0;
  do {
    reversed[digits++] = (char)('0' + pid % 10);
    pid /= 10;
  } while (pid);
  int path_length = 0;
  while (directory[path_length])
    path_length++;
  while (digits)
    directory[path_length++] = reversed[--digits];
  directory[path_length] = '\0';
  if (mkdir(directory, 0700) != 0)
    return 14;

  struct stat info;
  if (stat(directory, &info) != 0 || !S_ISDIR(info.st_mode))
    return 15;
  DIR *dir = opendir(directory);
  if (!dir)
    return 16;
  int directory_fd = dirfd(dir);
  if (directory_fd < 0 || fstat(directory_fd, &info) != 0 || !S_ISDIR(info.st_mode))
    return 17;
  if (fcntl(directory_fd, F_SETFD, FD_CLOEXEC) != 0 || (fcntl(directory_fd, F_GETFD) & FD_CLOEXEC) == 0)
    return 29;

  int fd = openat(directory_fd, "entry", O_CREAT | O_EXCL | O_RDWR, 0600);
  if (fd < 0 || write(fd, "data", 4) != 4)
    return 18;
  if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size != 4)
    return 19;

  int found_entry = 0;
  struct dirent *entry;
  while ((entry = readdir(dir)) != 0) {
    if (equal(entry->d_name, "entry"))
      found_entry = 1;
  }
  if (!found_entry || closedir(dir) != 0 || close(fd) != 0)
    return 20;

  char file_path[100];
  int file_length = append(file_path, directory, 0);
  file_length = append(file_path, "/entry", file_length);
  if (lstat(file_path, &info) != 0 || !S_ISREG(info.st_mode))
    return 21;
  if (unlink(file_path) != 0 || rmdir(directory) != 0)
    return 22;

  char cwd[1024];
  if (!getcwd(cwd, sizeof(cwd)) || !*cwd)
    return 23;
  if (fcntl(1, F_GETFD) < 0)
    return 24;

  /* The wait wrappers return the child PID and preserve its exit status. */
  int child = fork();
  if (child < 0)
    return 25;
  if (child == 0)
    _exit(37);
  int status = 0;
  if (waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status) != 37)
    return 26;

  return 0;
}
