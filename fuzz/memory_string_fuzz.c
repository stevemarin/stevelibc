#include "../include/stddef.h"
#include "../include/stdint.h"
#include "../include/string.h"

void *steve_memcpy(void *destination, const void *source, size_t count);
void *steve_memmove(void *destination, const void *source, size_t count);
void *steve_memset(void *destination, int byte, size_t count);
int steve_memcmp(const void *left, const void *right, size_t count);
void *steve_memchr(const void *memory, int byte, size_t count);
size_t steve_strlen(const char *string);
size_t steve_strnlen(const char *string, size_t maximum);
int steve_strcmp(const char *left, const char *right);
int steve_strncmp(const char *left, const char *right, size_t count);
char *steve_strcpy(char *destination, const char *source);
char *steve_strncpy(char *destination, const char *source, size_t count);
char *steve_strcat(char *destination, const char *source);
char *steve_strncat(char *destination, const char *source, size_t count);
char *steve_strchr(const char *string, int byte);
char *steve_strrchr(const char *string, int byte);
char *steve_strstr(const char *string, const char *substring);

enum { MAX_BYTES = 512, MAX_BUFFER = MAX_BYTES * 2 + 4 };

static void copy_input(unsigned char *output, size_t output_size, const uint8_t *data,
                       size_t size, size_t offset) {
  for (size_t i = 0; i < output_size; i++)
    output[i] = size ? data[(offset + i) % size] : 0;
}

static int compare_sign(int value) { return (value > 0) - (value < 0); }

static void fuzz_memory(const uint8_t *data, size_t size) {
  unsigned char custom_source[MAX_BUFFER];
  unsigned char host_source[MAX_BUFFER];
  unsigned char custom_output[MAX_BUFFER];
  unsigned char host_output[MAX_BUFFER];
  copy_input(custom_source, sizeof(custom_source), data, size, 0);
  copy_input(host_source, sizeof(host_source), data, size, 0);

  size_t source_offset = size ? data[0] % (MAX_BUFFER / 2) : 0;
  size_t destination_offset = size > 1 ? data[1] % (MAX_BUFFER / 2) : 0;
  size_t available = MAX_BUFFER - (source_offset > destination_offset ? source_offset : destination_offset);
  size_t count = size > 2 ? data[2] % (available + 1) : 0;

  for (size_t i = 0; i < MAX_BUFFER; i++) {
    custom_output[i] = (unsigned char)(i * 13U);
    host_output[i] = custom_output[i];
  }
  if (steve_memcpy(custom_output + destination_offset, custom_source + source_offset, count) !=
      custom_output + destination_offset)
    __builtin_trap();
  memcpy(host_output + destination_offset, host_source + source_offset, count);
  if (memcmp(custom_output, host_output, sizeof(host_output)) != 0)
    __builtin_trap();

  copy_input(custom_output, sizeof(custom_output), data, size, 0);
  memcpy(host_output, custom_output, sizeof(host_output));
  size_t move_source = size > 3 ? data[3] % (MAX_BUFFER / 2) : 0;
  size_t move_destination = size > 4 ? data[4] % (MAX_BUFFER / 2) : 0;
  available = MAX_BUFFER - (move_source > move_destination ? move_source : move_destination);
  size_t move_count = size > 5 ? data[5] % (available + 1) : 0;
  if (steve_memmove(custom_output + move_destination, custom_output + move_source, move_count) !=
      custom_output + move_destination)
    __builtin_trap();
  memmove(host_output + move_destination, host_output + move_source, move_count);
  if (memcmp(custom_output, host_output, sizeof(host_output)) != 0)
    __builtin_trap();

  int byte = size > 6 ? data[6] : 0;
  copy_input(custom_output, sizeof(custom_output), data, size, 0);
  memcpy(host_output, custom_output, sizeof(host_output));
  if (steve_memset(custom_output + destination_offset, byte, count) != custom_output + destination_offset)
    __builtin_trap();
  memset(host_output + destination_offset, byte, count);
  if (memcmp(custom_output, host_output, sizeof(host_output)) != 0 ||
      compare_sign(steve_memcmp(custom_output, custom_source, count)) !=
          compare_sign(memcmp(host_output, host_source, count)))
    __builtin_trap();

  void *custom_match = steve_memchr(custom_source + source_offset, byte, count);
  void *host_match = memchr(host_source + source_offset, byte, count);
  if ((custom_match == 0) != (host_match == 0) ||
      (custom_match && (unsigned char *)custom_match - custom_source !=
                           (unsigned char *)host_match - host_source))
    __builtin_trap();
}

static void make_string(char *output, size_t capacity, const uint8_t *data, size_t size,
                        size_t offset) {
  size_t length = size ? size / 2 : 0;
  if (length >= capacity)
    length = capacity - 1;
  for (size_t i = 0; i < length; i++)
    output[i] = (char)data[(offset + i) % size];
  output[length] = '\0';
}

static void fuzz_strings(const uint8_t *data, size_t size) {
  char left[MAX_BYTES + 1];
  char right[MAX_BYTES + 1];
  make_string(left, sizeof(left), data, size, 0);
  make_string(right, sizeof(right), data, size, size / 2);
  size_t limit = size ? data[0] % (MAX_BYTES + 1) : 0;
  int byte = size ? data[size - 1] : 0;

  if (steve_strlen(left) != strlen(left) ||
      steve_strnlen(left, limit) != strnlen(left, limit) ||
      compare_sign(steve_strcmp(left, right)) != compare_sign(strcmp(left, right)) ||
      compare_sign(steve_strncmp(left, right, limit)) != compare_sign(strncmp(left, right, limit)))
    __builtin_trap();

  char *custom_match = steve_strchr(left, byte);
  char *host_match = strchr(left, byte);
  if ((custom_match == 0) != (host_match == 0) ||
      (custom_match && custom_match - left != host_match - left))
    __builtin_trap();
  custom_match = steve_strrchr(left, byte);
  host_match = strrchr(left, byte);
  if ((custom_match == 0) != (host_match == 0) ||
      (custom_match && custom_match - left != host_match - left))
    __builtin_trap();
  custom_match = steve_strstr(left, right);
  host_match = strstr(left, right);
  if ((custom_match == 0) != (host_match == 0) ||
      (custom_match && custom_match - left != host_match - left))
    __builtin_trap();

  char custom[MAX_BUFFER];
  char host[MAX_BUFFER];
  if (steve_strcpy(custom, left) != custom)
    __builtin_trap();
  if (strcpy(host, left) != host || strcmp(custom, host) != 0)
    __builtin_trap();
  if (steve_strncpy(custom, right, limit) != custom ||
      strncpy(host, right, limit) != host ||
      memcmp(custom, host, sizeof(custom)) != 0)
    __builtin_trap();

  if (steve_strcpy(custom, left) != custom || strcpy(host, left) != host ||
      steve_strcat(custom, right) != custom || strcat(host, right) != host ||
      strcmp(custom, host) != 0)
    __builtin_trap();
  size_t append_limit = size > 1 ? data[1] % (MAX_BYTES + 1) : 0;
  if (steve_strcpy(custom, left) != custom || strcpy(host, left) != host ||
      steve_strncat(custom, right, append_limit) != custom ||
      strncat(host, right, append_limit) != host || strcmp(custom, host) != 0)
    __builtin_trap();
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  fuzz_memory(data, size);
  fuzz_strings(data, size);
  return 0;
}
