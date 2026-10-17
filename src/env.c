#include "../include/errno.h"
#include "../include/stdlib.h"
#include "../include/unistd.h"

static char **owned_environment;
static unsigned long environment_count;
static unsigned long environment_capacity;
static unsigned char environment_lock;

/*
 * Startup's environ points into the initial process stack. Reads can use it
 * directly, but a mutator first clones both the pointer array and its strings
 * so edits never overwrite kernel-provided startup storage.
 */
static void lock_environment(void) {
  while (__atomic_test_and_set(&environment_lock, __ATOMIC_ACQUIRE))
    __asm__ volatile("pause");
}

static void unlock_environment(void) { __atomic_clear(&environment_lock, __ATOMIC_RELEASE); }

static unsigned long string_length(const char *string) {
  unsigned long length = 0;
  while (string[length])
    length++;
  return length;
}

static int name_matches(const char *entry, const char *name, unsigned long name_length) {
  for (unsigned long i = 0; i < name_length; i++) {
    if (!entry[i] || entry[i] != name[i])
      return 0;
  }
  return entry[name_length] == '=';
}

static int environment_index(const char *name, unsigned long name_length) {
  if (!environ)
    return -1;
  for (unsigned long i = 0; environ[i]; i++) {
    if (name_matches(environ[i], name, name_length))
      return (int)i;
  }
  return -1;
}

static int copy_environment(void) {
  if (owned_environment)
    return 0;

  unsigned long count = 0;
  while (environ && environ[count])
    count++;
  char **copy = (char **)malloc((count + 1) * sizeof(char *));
  if (!copy) {
    errno = ENOMEM;
    return -1;
  }
  for (unsigned long i = 0; i < count; i++) {
    unsigned long length = string_length(environ[i]) + 1;
    copy[i] = (char *)malloc(length);
    if (!copy[i]) {
      while (i > 0)
        free(copy[--i]);
      free(copy);
      errno = ENOMEM;
      return -1;
    }
    for (unsigned long j = 0; j < length; j++)
      copy[i][j] = environ[i][j];
  }
  copy[count] = 0;
  owned_environment = copy;
  environment_count = count;
  environment_capacity = count + 1;
  environ = copy;
  return 0;
}

/* Grow geometrically so repeated setenv calls do not realloc on every insert. */
static int ensure_capacity(unsigned long needed) {
  if (needed <= environment_capacity)
    return 0;
  unsigned long capacity = environment_capacity ? environment_capacity : 4;
  while (capacity < needed) {
    if (capacity > ~0UL / 2 / sizeof(char *)) {
      errno = ENOMEM;
      return -1;
    }
    capacity *= 2;
  }
  char **grown = (char **)realloc(owned_environment, capacity * sizeof(char *));
  if (!grown) {
    errno = ENOMEM;
    return -1;
  }
  owned_environment = grown;
  environment_capacity = capacity;
  environ = grown;
  return 0;
}

char *getenv(const char *name) {
  if (!name || !*name)
    return 0;
  unsigned long length = string_length(name);
  for (unsigned long i = 0; name[i]; i++) {
    if (name[i] == '=')
      return 0;
  }

  lock_environment();
  int index = environment_index(name, length);
  /*
   * Like the conventional API, the returned pointer refers into environ and
   * can be invalidated by a later mutation; the lock protects only the lookup.
   */
  char *value = index < 0 ? 0 : environ[index] + length + 1;
  unlock_environment();
  return value;
}

int setenv(const char *name, const char *value, int overwrite) {
  if (!name || !*name || !value) {
    errno = EINVAL;
    return -1;
  }
  unsigned long name_length = string_length(name);
  for (unsigned long i = 0; i < name_length; i++) {
    if (name[i] == '=') {
      errno = EINVAL;
      return -1;
    }
  }

  lock_environment();
  int index = environment_index(name, name_length);
  /* POSIX leaves an existing assignment untouched when overwrite is zero. */
  if (index >= 0 && !overwrite) {
    unlock_environment();
    return 0;
  }
  /* Build owned storage before publishing the replacement into environ. */
  if (copy_environment() != 0) {
    unlock_environment();
    return -1;
  }
  if (index < 0 && ensure_capacity(environment_count + 2) != 0) {
    unlock_environment();
    return -1;
  }

  unsigned long value_length = string_length(value);
  if (name_length > ~0UL - value_length - 2) {
    unlock_environment();
    errno = ENOMEM;
    return -1;
  }
  char *entry = (char *)malloc(name_length + value_length + 2);
  if (!entry) {
    unlock_environment();
    errno = ENOMEM;
    return -1;
  }
  unsigned long at = 0;
  for (; at < name_length; at++)
    entry[at] = name[at];
  entry[at++] = '=';
  for (unsigned long i = 0; i <= value_length; i++)
    entry[at + i] = value[i];

  if (index >= 0) {
    free(owned_environment[index]);
    owned_environment[index] = entry;
  } else {
    owned_environment[environment_count++] = entry;
    owned_environment[environment_count] = 0;
  }
  unlock_environment();
  return 0;
}

int unsetenv(const char *name) {
  if (!name || !*name) {
    errno = EINVAL;
    return -1;
  }
  unsigned long length = string_length(name);
  for (unsigned long i = 0; i < length; i++) {
    if (name[i] == '=') {
      errno = EINVAL;
      return -1;
    }
  }

  lock_environment();
  if (copy_environment() != 0) {
    unlock_environment();
    return -1;
  }
  int index;
  while ((index = environment_index(name, length)) >= 0) {
    /* POSIX permits duplicate names in environ; remove every matching entry. */
    free(owned_environment[index]);
    for (unsigned long i = (unsigned long)index; i < environment_count; i++)
      owned_environment[i] = owned_environment[i + 1];
    environment_count--;
  }
  unlock_environment();
  return 0;
}

int clearenv(void) {
  lock_environment();
  if (copy_environment() != 0) {
    unlock_environment();
    return -1;
  }
  for (unsigned long i = 0; i < environment_count; i++)
    free(owned_environment[i]);
  /* Keep the vector allocated so a later setenv can reuse it. */
  environment_count = 0;
  owned_environment[0] = 0;
  unlock_environment();
  return 0;
}
