#include "../include/errno.h"
#include "../include/stdlib.h"
#include "../include/sys/wait.h"
#include "../include/unistd.h"

static int lifecycle_pipe = -1;
static int initialization_state;
static int dso_one;
static int dso_two;

static void write_marker(char marker) {
  if (lifecycle_pipe >= 0)
    (void)write(lifecycle_pipe, &marker, 1);
}

static void preinitializer(void) { initialization_state = 1; }

static void constructor(void) { initialization_state = initialization_state == 1 ? 2 : -1; }

static void destructor(void) {
  if (initialization_state == 2)
    write_marker('D');
  else
    write_marker('!');
}

static void plain_callback(void);

static void late_callback(void) { write_marker('X'); }

static void plain_callback(void) {
  write_marker('A');
  /* exit() must process callbacks registered by a callback it is running. */
  if (atexit(late_callback) != 0)
    write_marker('!');
}

static void cxa_one(void *argument) {
  if (argument == &dso_one)
    write_marker('B');
  else
    write_marker('!');
}

static void cxa_two(void *argument) {
  if (argument == &dso_two)
    write_marker('C');
  else
    write_marker('!');
}

__attribute__((section(".preinit_array"), used)) static void (*const test_preinit_entry)(void) = preinitializer;

__attribute__((constructor, used)) static void test_constructor_entry(void) { constructor(); }
__attribute__((destructor, used)) static void test_destructor_entry(void) { destructor(); }

/*
 * Verify both startup ordering and normal-exit unwinding. The child reports
 * callbacks through a pipe so the parent can inspect order after it is gone.
 */
int main(void) {
  if (initialization_state != 2)
    return 1;
  errno = 0;
  if (atexit(0) != -1 || errno != EINVAL)
    return 2;
  errno = 0;
  if (__cxa_atexit(0, 0, 0) != -1 || errno != EINVAL)
    return 3;

  int result_pipe[2];
  if (pipe(result_pipe) != 0)
    return 4;

  int child = fork();
  if (child < 0)
    return 5;
  if (child == 0) {
    close(result_pipe[0]);
    lifecycle_pipe = result_pipe[1];
    if (atexit(plain_callback) != 0 || __cxa_atexit(cxa_one, &dso_one, &dso_one) != 0 ||
        __cxa_atexit(cxa_two, &dso_two, &dso_two) != 0)
      _exit(6);

    /* Finalizing one DSO must not run another object's callback. A repeated
     * request is harmless because callbacks are marked before invocation. */
    __cxa_finalize(&dso_one);
    __cxa_finalize(&dso_one);
    write_marker('Y');
    __cxa_finalize(0);
    write_marker('Z');
    exit(0);
  }

  close(result_pipe[1]);
  char markers[8];
  int count = 0;
  while (count < (int)sizeof(markers)) {
    ssize_t bytes = read(result_pipe[0], markers + count, sizeof(markers) - (unsigned long)count);
    if (bytes < 0)
      return 7;
    if (bytes == 0)
      break;
    count += (int)bytes;
  }
  close(result_pipe[0]);

  int status = 0;
  if (waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
    return 8;
  if (count != 7 || markers[0] != 'B' || markers[1] != 'Y' || markers[2] != 'C' || markers[3] != 'A' ||
      markers[4] != 'X' || markers[5] != 'Z' || markers[6] != 'D')
    return 9;
  return 0;
}
