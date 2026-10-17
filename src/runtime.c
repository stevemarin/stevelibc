#include "../include/errno.h"
#include "../include/stdio.h"
#include "../include/stdlib.h"
#include "../include/syscall.h"

/*
 * C and C++ register process cleanup through different interfaces:
 * atexit callbacks take no argument, while __cxa_atexit callbacks receive an
 * object pointer and an owning DSO handle. One ordered table lets both obey
 * the same reverse-registration order during normal process exit.
 */
typedef struct {
  void (*plain)(void); /* atexit callback form */
  void (*cxa)(void *); /* C++ destructor callback form */
  void *argument; /* Object pointer passed only to cxa */
  void *dso_handle; /* Owner used to filter __cxa_finalize */
  int is_cxa; /* Selects the callback signature */
  int called; /* Prevents repeat execution during reentrant finalization */
} exit_callback;

static exit_callback callbacks[ATEXIT_MAX_FUNCS];
static int callback_count;
static unsigned char callback_lock;

/* The executable's own handle is a stable token for __cxa_finalize filtering. */
void *__dso_handle = &__dso_handle;

/* These boundaries are normally supplied by the ELF linker script. */
extern void (*__fini_array_start[])(void) __attribute__((weak));
extern void (*__fini_array_end[])(void) __attribute__((weak));

static void lock_callbacks(void) {
  while (__atomic_test_and_set(&callback_lock, __ATOMIC_ACQUIRE))
    __asm__ volatile("pause");
}

static void unlock_callbacks(void) { __atomic_clear(&callback_lock, __ATOMIC_RELEASE); }

static int reserve_callback(exit_callback callback) {
  lock_callbacks();
  if (callback_count == ATEXIT_MAX_FUNCS) {
    unlock_callbacks();
    errno = ENOMEM;
    return -1;
  }
  callbacks[callback_count++] = callback;
  unlock_callbacks();
  return 0;
}

int atexit(void_func func) {
  if (!func) {
    errno = EINVAL;
    return -1;
  }
  exit_callback callback = {.plain = func};
  return reserve_callback(callback);
}

int __cxa_atexit(void (*func)(void *), void *argument, void *dso_handle) {
  if (!func) {
    errno = EINVAL;
    return -1;
  }
  exit_callback callback = {
      .cxa = func,
      .argument = argument,
      .dso_handle = dso_handle,
      .is_cxa = 1,
  };
  return reserve_callback(callback);
}

/*
 * Mark a callback before calling it and release the lock around user code.
 * That ordering makes reentrant __cxa_finalize safe: the callback already in
 * progress cannot be selected again, and a callback may register another one.
 */
void __cxa_finalize(void *dso_handle) {
  for (;;) {
    exit_callback callback = {0};
    int found = 0;
    lock_callbacks();
    for (int i = callback_count - 1; i >= 0; i--) {
      exit_callback *candidate = &callbacks[i];
      if (!candidate->called && (!dso_handle || candidate->dso_handle == dso_handle)) {
        candidate->called = 1;
        callback = *candidate;
        found = 1;
        break;
      }
    }
    unlock_callbacks();

    if (!found)
      return;
    if (callback.is_cxa)
      callback.cxa(callback.argument);
    else
      callback.plain();
  }
}

/*
 * The ELF fini array is traversed opposite to the init array: dependencies and
 * constructors are unwound in reverse order. Missing weak boundaries mean the
 * linker supplied no array, which is a valid case for small standalone tests.
 */
static void run_fini_array(void) {
  if (!__fini_array_start || !__fini_array_end)
    return;
  for (void (**entry)(void) = __fini_array_end; entry > __fini_array_start;) {
    void (*function)(void) = *--entry;
    if (function)
      function();
  }
}

extern void __libc_run_dynamic_fini(void);

void exit(int status) {
  /*
   * User callbacks (including C++ static-object destructors) run before ELF
   * fini entries. Fini trampolines may call __cxa_finalize again; called bits
   * make those repeated finalization requests harmless.
   */
  __cxa_finalize(0);
  __libc_run_dynamic_fini();
  run_fini_array();

  /* Cleanup code may have printed, so drain stdio only after it completes. */
  (void)fflush(0);
  syscall1(SYS_exit_group, status);
  __builtin_unreachable();
}
