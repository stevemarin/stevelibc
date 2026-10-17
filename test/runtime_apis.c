#include "../include/errno.h"
#include "../include/sys/mman.h"
#include "../include/unistd.h"

/*
 * Exercise the small POSIX descriptor surface and Linux memory mapping calls
 * without relying on stdio or an external runtime.
 */
int main(void) {
  /* Use a PID-specific path so repeated or concurrent test runs do not collide. */
  char directory[64] = "/tmp/stevelibc-runtime-";
  unsigned long pid = (unsigned long)getpid();
  char reversed[20];
  int digits = 0;
  do {
    reversed[digits++] = (char)('0' + pid % 10);
    pid /= 10;
  } while (pid);
  int position = 0;
  while (directory[position])
    position++;
  while (digits > 0)
    directory[position++] = reversed[--digits];
  directory[position] = '\0';

  char path[80];
  int prefix_length = 0;
  while (directory[prefix_length]) {
    path[prefix_length] = directory[prefix_length];
    prefix_length++;
  }
  const char suffix[] = "/data";
  for (int i = 0; i < 6; i++)
    path[prefix_length + i] = suffix[i];

  if (mkdir(directory, 0700) != 0)
    return 1;
  int fd = open(path, O_CREAT | O_EXCL | O_RDWR, 0600);
  if (fd < 0)
    return 2;
  if (write(fd, "abc", 3) != 3 || lseek(fd, 0, SEEK_SET) != 0)
    return 3;

  /* dup shares the same open-file description, including its current offset. */
  int duplicate = dup(fd);
  if (duplicate < 0 || lseek(duplicate, 1, SEEK_SET) != 1)
    return 4;
  char bytes[4] = {0};
  if (read(fd, bytes, 2) != 2 || bytes[0] != 'b' || bytes[1] != 'c')
    return 5;
  if (access(path, F_OK) != 0 || access(path, R_OK | W_OK) != 0)
    return 6;
  if (close(duplicate) != 0 || close(fd) != 0)
    return 7;

  int descriptors[2];
  /* A pipe transfers one byte from its write end to its read end. */
  if (pipe(descriptors) != 0)
    return 8;
  if (write(descriptors[1], "p", 1) != 1 || read(descriptors[0], bytes, 1) != 1 || bytes[0] != 'p')
    return 9;
  if (dup2(descriptors[0], descriptors[0]) != descriptors[0])
    return 10;
  if (close(descriptors[0]) != 0 || close(descriptors[1]) != 0)
    return 11;
  errno = 0;
  if (pipe(0) != -1 || errno != EFAULT)
    return 21;

  long page_size = (long)getauxval(AT_PAGESZ);
  if (page_size <= 0)
    return 12;
  char *mapping =
      (char *)mmap(0, (unsigned long)page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mapping == MAP_FAILED)
    return 13;
  mapping[0] = 'm';
  mapping[page_size - 1] = 'z';
  if (mprotect(mapping, (unsigned long)page_size, PROT_READ) != 0)
    return 14;
  if (mprotect(mapping, (unsigned long)page_size, PROT_READ | PROT_WRITE) != 0)
    return 15;
  if (mapping[0] != 'm' || mapping[page_size - 1] != 'z')
    return 16;
  if (munmap(mapping, (unsigned long)page_size) != 0)
    return 17;

  errno = 0;
  /* A zero-length mapping is invalid and should use normal errno conversion. */
  if (mmap(0, 0, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) != MAP_FAILED || errno != EINVAL)
    return 18;

  if (unlink(path) != 0 || rmdir(directory) != 0)
    return 19;
  if (getpid() <= 0 || getppid() < 0)
    return 20;

  return 0;
}
