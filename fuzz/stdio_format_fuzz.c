#include "../include/stddef.h"
#include "../include/stdint.h"
#include "../include/stdio.h"
#include "../include/string.h"

int stevelibc_fuzz_snprintf(char *buffer, unsigned long size, const char *format, ...);
int stevelibc_fuzz_sscanf(const char *input, const char *format, ...);

enum { OUTPUT_SIZE = 512, STRING_SIZE = 256 };

static uint64_t input_value(const uint8_t *data, size_t size) {
  uint64_t value = 0;
  size_t count = size < sizeof(value) ? size : sizeof(value);
  for (size_t i = 0; i < count; i++)
    value |= (uint64_t)data[i] << (i * 8);
  return value;
}

static void input_string(char *string, const uint8_t *data, size_t size) {
  size_t count = size < STRING_SIZE - 1 ? size : STRING_SIZE - 1;
  for (size_t i = 0; i < count; i++)
    string[i] = data[i] ? (char)data[i] : 'x';
  string[count] = '\0';
}

static void check_format(const uint8_t *data, size_t size, unsigned int choice,
                         unsigned long output_limit, const char *string, uint64_t value) {
  char custom[OUTPUT_SIZE];
  char host[OUTPUT_SIZE];
  memset(custom, 0xa5, sizeof(custom));
  memset(host, 0xa5, sizeof(host));
  int custom_count;
  int host_count;
  int precision = size ? data[size - 1] % 180 : 0;
  int width = size > 1 ? data[size - 2] % 60 : 0;

  switch (choice % 13) {
  case 0:
    custom_count = stevelibc_fuzz_snprintf(custom, output_limit, "%d", (int)value);
    host_count = snprintf(host, output_limit, "%d", (int)value);
    break;
  case 1:
    custom_count = stevelibc_fuzz_snprintf(custom, output_limit, "%#08x", (unsigned int)value);
    host_count = snprintf(host, output_limit, "%#08x", (unsigned int)value);
    break;
  case 2:
    custom_count = stevelibc_fuzz_snprintf(custom, output_limit, "%lld", (long long)value);
    host_count = snprintf(host, output_limit, "%lld", (long long)value);
    break;
  case 3:
    custom_count = stevelibc_fuzz_snprintf(custom, output_limit, "%llu", (unsigned long long)value);
    host_count = snprintf(host, output_limit, "%llu", (unsigned long long)value);
    break;
  case 4:
    custom_count = stevelibc_fuzz_snprintf(custom, output_limit, "%#o", (unsigned int)value);
    host_count = snprintf(host, output_limit, "%#o", (unsigned int)value);
    break;
  case 5:
    custom_count = stevelibc_fuzz_snprintf(custom, output_limit, "%+010d", (int)value);
    host_count = snprintf(host, output_limit, "%+010d", (int)value);
    break;
  case 6:
    custom_count = stevelibc_fuzz_snprintf(custom, output_limit, "%.*s", precision, string);
    host_count = snprintf(host, output_limit, "%.*s", precision, string);
    break;
  case 7:
    custom_count = stevelibc_fuzz_snprintf(custom, output_limit, "%-*.*s", width, precision, string);
    host_count = snprintf(host, output_limit, "%-*.*s", width, precision, string);
    break;
  case 8:
    custom_count = stevelibc_fuzz_snprintf(custom, output_limit, "%c", (int)value);
    host_count = snprintf(host, output_limit, "%c", (int)value);
    break;
  case 9:
    custom_count = stevelibc_fuzz_snprintf(custom, output_limit, "%p", (void *)(uintptr_t)value);
    host_count = snprintf(host, output_limit, "%p", (void *)(uintptr_t)value);
    break;
  case 10:
    custom_count = stevelibc_fuzz_snprintf(custom, output_limit, "%%:%zu", (size_t)value);
    host_count = snprintf(host, output_limit, "%%:%zu", (size_t)value);
    break;
  case 11:
    custom_count = stevelibc_fuzz_snprintf(custom, output_limit, "%.0d", (int)value);
    host_count = snprintf(host, output_limit, "%.0d", (int)value);
    break;
  default:
    custom_count = stevelibc_fuzz_snprintf(custom, output_limit, "%08llu", (unsigned long long)value);
    host_count = snprintf(host, output_limit, "%08llu", (unsigned long long)value);
    break;
  }

  if (custom_count != host_count || memcmp(custom, host, sizeof(custom)) != 0) {
    fprintf(stderr, "format choice=%u limit=%lu custom=%d [%s] host=%d [%s]\n", choice % 13,
            output_limit, custom_count, custom, host_count, host);
    __builtin_trap();
  }
}

static void check_scan(const uint8_t *data, size_t size, const char *input) {
  unsigned int choice = size > 1 ? data[1] % 7 : 0;
  int custom_count;
  int host_count;
  int custom_int = 0x12345678;
  int host_int = custom_int;
  unsigned int custom_unsigned = 0x87654321U;
  unsigned int host_unsigned = custom_unsigned;
  long long custom_long_long = 0x123456789abcdefLL;
  long long host_long_long = custom_long_long;
  char custom_text[32];
  char host_text[32];
  memset(custom_text, 0xa5, sizeof(custom_text));
  memset(host_text, 0xa5, sizeof(host_text));

  switch (choice) {
  case 0:
    custom_count = stevelibc_fuzz_sscanf(input, "%9d", &custom_int);
    host_count = sscanf(input, "%9d", &host_int);
    if (custom_count != host_count || custom_int != host_int) {
      fprintf(stderr, "sscanf %%d input=[%s] custom=%d/%d host=%d/%d\n", input, custom_count, custom_int,
              host_count, host_int);
      __builtin_trap();
    }
    break;
  case 1:
    custom_count = stevelibc_fuzz_sscanf(input, "%9u", &custom_unsigned);
    host_count = sscanf(input, "%9u", &host_unsigned);
    if (custom_count != host_count || custom_unsigned != host_unsigned)
      __builtin_trap();
    break;
  case 2:
    custom_count = stevelibc_fuzz_sscanf(input, "%8x", &custom_unsigned);
    host_count = sscanf(input, "%8x", &host_unsigned);
    if (custom_count != host_count || custom_unsigned != host_unsigned)
      __builtin_trap();
    break;
  case 3:
    custom_count = stevelibc_fuzz_sscanf(input, "%18lld", &custom_long_long);
    host_count = sscanf(input, "%18lld", &host_long_long);
    if (custom_count != host_count || custom_long_long != host_long_long)
      __builtin_trap();
    break;
  case 4:
    custom_count = stevelibc_fuzz_sscanf(input, "%31s", custom_text);
    host_count = sscanf(input, "%31s", host_text);
    if (custom_count != host_count || memcmp(custom_text, host_text, sizeof(custom_text)) != 0)
      __builtin_trap();
    break;
  case 5:
    custom_count = stevelibc_fuzz_sscanf(input, "%3c", custom_text);
    host_count = sscanf(input, "%3c", host_text);
    if (custom_count != host_count || memcmp(custom_text, host_text, 3) != 0)
      __builtin_trap();
    break;
  default:
    custom_count = stevelibc_fuzz_sscanf(input, "%9i", &custom_int);
    host_count = sscanf(input, "%9i", &host_int);
    if (custom_count != host_count || custom_int != host_int)
      __builtin_trap();
    break;
  }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  char string[STRING_SIZE];
  input_string(string, data, size);
  uint64_t value = input_value(data, size);
  unsigned int choice = size ? data[0] : 0;
  unsigned long limit = size > 1 ? data[1] % (OUTPUT_SIZE + 1) : 0;
  check_format(data, size, choice, limit, string, value);
  check_scan(data, size, string);
  return 0;
}
