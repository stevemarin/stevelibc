#include "../include/errno.h"
#include "../include/fcntl.h"
#include "../include/signal.h"
#include "../include/stdlib.h"
#include "../include/sys/stat.h"
#include "../include/sys/wait.h"
#include "../include/unistd.h"

static int same_string(const char *left, const char *right) {
  while (*left && *left == *right) {
    left++;
    right++;
  }
  return *left == *right;
}

static int append(char *destination, int at, const char *source) {
  while (*source)
    destination[at++] = *source++;
  destination[at] = '\0';
  return at;
}

static int append_pid(char *destination, const char *prefix) {
  int at = append(destination, 0, prefix);
  unsigned long pid = (unsigned long)getpid();
  char reverse[20];
  int digits = 0;
  do {
    reverse[digits++] = (char)('0' + pid % 10);
    pid /= 10;
  } while (pid);
  while (digits)
    destination[at++] = reverse[--digits];
  destination[at] = '\0';
  return at;
}

/*
 * The test executable doubles as an exec target. Re-entering the same binary
 * proves exec replaced the child image rather than merely returning success.
 */
int main(int argc, char **argv) {
  if (argc == 2 && same_string(argv[1], "--exec-child"))
    return 73;

  char directory[96];
  append_pid(directory, "/tmp/stevelibc-procfs-");
  if (mkdir(directory, 0700) != 0)
    return 1;

  /* A directory descriptor anchors subsequent relative paths independently
   * of the process's current directory. */
  int directory_fd = open(directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (directory_fd < 0)
    return 2;
  if (mkdirat(directory_fd, "nested", 0700) != 0 || faccessat(directory_fd, "nested", F_OK) != 0)
    return 39;

  /* umask masks requested creation bits; restore the inherited policy
   * immediately so later tests and process behavior are not affected. */
  mode_t old_mask = umask(0077);
  int file_fd = openat(directory_fd, "original", O_CREAT | O_EXCL | O_RDWR, 0666);
  umask(old_mask);
  if (file_fd < 0)
    return 3;

  struct stat file_info;
  if (fstat(file_fd, &file_info) != 0 || (file_info.st_mode & 0777) != 0600)
    return 4;
  if (write(file_fd, "sample", 6) != 6 || fsync(file_fd) != 0)
    return 5;
  char original_path[128];
  int original_length = append(original_path, 0, directory);
  append(original_path, original_length, "/original");
  if (truncate(original_path, 5) != 0 || fstat(file_fd, &file_info) != 0 || file_info.st_size != 5)
    return 40;
  if (ftruncate(file_fd, 3) != 0 || fstat(file_fd, &file_info) != 0 || file_info.st_size != 3)
    return 6;
  if (fchmod(file_fd, 0640) != 0 || fstat(file_fd, &file_info) != 0 || (file_info.st_mode & 0777) != 0640)
    return 7;

  /* Hard links are two directory names for one inode. Changes through either
   * name therefore identify the same underlying file. */
  if (linkat(directory_fd, "original", directory_fd, "hardlink", 0) != 0)
    return 8;
  struct stat linked_info;
  if (fstatat(directory_fd, "hardlink", &linked_info, 0) != 0 || linked_info.st_ino != file_info.st_ino)
    return 9;
  if (renameat(directory_fd, "hardlink", directory_fd, "renamed") != 0 ||
      fstatat(directory_fd, "renamed", &linked_info, 0) != 0 || linked_info.st_ino != file_info.st_ino)
    return 10;

  /* A symlink stores text, not a second reference to the target inode.
   * readlink copies that text without adding a string terminator. */
  if (symlinkat("original", directory_fd, "symbolic") != 0)
    return 11;
  char target[128] = {0};
  target[8] = '!';
  ssize_t target_length = readlinkat(directory_fd, "symbolic", target, sizeof(target));
  if (target_length != 8 || target[8] != '!')
    return 12;
  target[8] = '\0';
  if (!same_string(target, "original"))
    return 52;
  if (fstatat(directory_fd, "symbolic", &linked_info, AT_SYMLINK_NOFOLLOW) != 0 || !S_ISLNK(linked_info.st_mode))
    return 13;
  if (fstatat(directory_fd, "symbolic", &linked_info, 0) != 0 || linked_info.st_ino != file_info.st_ino)
    return 14;

  /* The non-at link and symlink calls are convenient wrappers around paths;
   * linkat/symlinkat above show how the same operations bind to directory fds. */
  char plain_link[128];
  int plain_link_length = append(plain_link, 0, directory);
  append(plain_link, plain_link_length, "/plain-link");
  char plain_symlink[128];
  int plain_symlink_length = append(plain_symlink, 0, directory);
  append(plain_symlink, plain_symlink_length, "/plain-symlink");
  if (link(original_path, plain_link) != 0 || symlink(original_path, plain_symlink) != 0 ||
      readlink(plain_symlink, target, sizeof(target)) != (ssize_t)(original_length + 9))
    return 41;

  /* Also exercise pathname wrappers; they should have the same kernel error
   * and metadata behavior as the directory-relative variants. */
  char renamed_path[128];
  int renamed_length = append(renamed_path, 0, directory);
  renamed_length = append(renamed_path, renamed_length, "/renamed");
  if (chmod(renamed_path, 0600) != 0 || stat(renamed_path, &linked_info) != 0 || (linked_info.st_mode & 0777) != 0600)
    return 15;
  char moved_path[128];
  int moved_length = append(moved_path, 0, directory);
  moved_length = append(moved_path, moved_length, "/moved");
  if (rename(renamed_path, moved_path) != 0 || access(moved_path, F_OK) != 0)
    return 16;

  /* execvp searches every PATH element and must not downgrade a permission
   * error to ENOENT when no later candidate can run. */
  char denied_path[128];
  int denied_length = append(denied_path, 0, directory);
  append(denied_path, denied_length, "/denied");
  int denied_fd = open(denied_path, O_CREAT | O_EXCL | O_WRONLY, 0600);
  if (denied_fd < 0 || close(denied_fd) != 0 || chmod(denied_path, 0600) != 0 || setenv("PATH", directory, 1) != 0)
    return 49;
  char *denied_arguments[] = {"denied", 0};
  errno = 0;
  if (execvp("denied", denied_arguments) != -1 || errno != EACCES)
    return 50;

  /* fchdir uses an open directory object, so it can restore a location even
   * after pathname resolution has changed the process working directory. */
  int original_cwd_fd = open(".", O_RDONLY | O_DIRECTORY);
  char original_cwd[256];
  if (original_cwd_fd < 0 || !getcwd(original_cwd, sizeof(original_cwd)))
    return 17;
  if (chdir(directory) != 0)
    return 18;
  char current_cwd[256];
  if (!getcwd(current_cwd, sizeof(current_cwd)) || !same_string(current_cwd, directory))
    return 19;
  if (fchdir(original_cwd_fd) != 0 || !getcwd(current_cwd, sizeof(current_cwd)) ||
      !same_string(current_cwd, original_cwd))
    return 20;
  if (close(original_cwd_fd) != 0)
    return 21;

  if (close(file_fd) != 0)
    return 22;
  errno = 0;
  if (fsync(-1) != -1 || errno != EBADF)
    return 53;
  if (unlinkat(directory_fd, "original", 0) != 0 || unlinkat(directory_fd, "symbolic", 0) != 0 ||
      unlink(moved_path) != 0 || unlink(plain_link) != 0 || unlink(plain_symlink) != 0 || unlink(denied_path) != 0)
    return 23;
  errno = 0;
  if (unlinkat(directory_fd, "does-not-exist", 0) != -1 || errno != ENOENT)
    return 24;
  if (unlinkat(directory_fd, "nested", AT_REMOVEDIR) != 0 || close(directory_fd) != 0 || rmdir(directory) != 0)
    return 25;

  /* Process-group/session IDs are kernel-managed relationships, not values
   * derived by libc. setpgid(0, 0) makes this child its own group leader. */
  int child = fork();
  if (child < 0)
    return 26;
  if (child == 0) {
    if (setpgid(0, 0) != 0 || getpgid(0) != getpid())
      _exit(74);
    _exit(0);
  }
  int status = 0;
  if (waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
    return 27;
  errno = 0;
  if (waitpid(child, &status, 0) != -1 || errno != ECHILD)
    return 54;

  /* A fresh child is not a process-group leader yet, so setsid can make it
   * both a session leader and a new process-group leader. */
  child = fork();
  if (child < 0)
    return 28;
  if (child == 0) {
    pid_t session = setsid();
    _exit(session == getpid() && getsid(0) == getpid() ? 0 : 75);
  }
  if (waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
    return 29;
  if (getpgid(0) < 0 || getsid(0) < 0 || kill(getpid(), 0) != 0)
    return 30;

  /* WNOHANG returns zero while a known child is still waiting on its gate. */
  int gate[2];
  if (pipe(gate) != 0)
    return 42;
  child = fork();
  if (child < 0)
    return 43;
  if (child == 0) {
    close(gate[1]);
    char release;
    if (read(gate[0], &release, 1) != 1)
      _exit(78);
    _exit(0);
  }
  close(gate[0]);
  if (waitpid(child, &status, WNOHANG) != 0)
    return 44;
  if (write(gate[1], "g", 1) != 1 || close(gate[1]) != 0 || waitpid(child, &status, 0) != child ||
      !WIFEXITED(status) || WEXITSTATUS(status) != 0)
    return 45;

  /* A zero-signal probe only checks existence; SIGTERM below verifies signal
   * delivery and the signaled-child wait-status encoding. */
  if (pipe(gate) != 0)
    return 46;
  child = fork();
  if (child < 0)
    return 47;
  if (child == 0) {
    close(gate[0]);
    if (write(gate[1], "r", 1) != 1)
      _exit(79);
    for (;;)
      __asm__ volatile("pause");
  }
  close(gate[1]);
  char ready;
  if (read(gate[0], &ready, 1) != 1 || close(gate[0]) != 0 || kill(child, SIGTERM) != 0 ||
      waitpid(child, &status, 0) != child || !WIFSIGNALED(status) || WTERMSIG(status) != SIGTERM)
    return 48;

  /* Verify execv replaces a forked child's image and preserves its argv. */
  char executable[512];
  ssize_t executable_length = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
  if (executable_length <= 0 || executable_length >= (ssize_t)sizeof(executable))
    return 31;
  executable[executable_length] = '\0';
  char *exec_arguments[] = {executable, "--exec-child", 0};
  child = fork();
  if (child < 0)
    return 32;
  if (child == 0) {
    execv(executable, exec_arguments);
    _exit(76);
  }
  if (waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status) != 73)
    return 33;

  /* execvp locates a basename through PATH; set PATH to this test's directory
   * so this does not depend on host-installed commands. */
  int slash = -1;
  for (int i = 0; executable[i]; i++) {
    if (executable[i] == '/')
      slash = i;
  }
  if (slash < 0)
    return 34;
  char executable_directory[512];
  for (int i = 0; i < slash; i++)
    executable_directory[i] = executable[i];
  executable_directory[slash] = '\0';
  if (setenv("PATH", executable_directory, 1) != 0)
    return 35;
  char *program_name = executable + slash + 1;
  char *search_arguments[] = {program_name, "--exec-child", 0};
  child = fork();
  if (child < 0)
    return 36;
  if (child == 0) {
    execvp(program_name, search_arguments);
    _exit(77);
  }
  if (waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status) != 73)
    return 37;

  errno = 0;
  if (execvp("stevelibc-command-that-does-not-exist", exec_arguments) != -1 || errno != ENOENT)
    return 38;
  errno = 0;
  if (kill(-999999, 0) != -1 || errno != ESRCH)
    return 51;
  return 0;
}
