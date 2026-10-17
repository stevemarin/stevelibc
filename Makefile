# ==============================================================================
# MAKEFILE : Build Automation Script for Custom Minimal x86_64 Linux Libc
# Structure: Headers in ./include, Sources in ./src, Objects in ./build
# ==============================================================================

CC = gcc
AR = ar
FUZZ_CC ?= clang
FUZZ_LDFLAGS ?= -fuse-ld=bfd

# Keep compiler built-ins disabled: byte-loop implementations of libc routines
# must not be rewritten into calls to those same routines.
CFLAGS = -ffreestanding -fno-builtin -nostdlib -nostartfiles -O2 -Wall -Wextra -Werror -Iinclude

SRC_DIR = src
BUILD_DIR = build

# Define object file paths inside the ./build directory
LIB_OBJS = $(BUILD_DIR)/stevelibc.o \
           $(BUILD_DIR)/ctype.o \
           $(BUILD_DIR)/stdio.o \
           $(BUILD_DIR)/malloc.o \
           $(BUILD_DIR)/errno.o \
           $(BUILD_DIR)/signal.o \
           $(BUILD_DIR)/threads.o \
           $(BUILD_DIR)/memory.o \
           $(BUILD_DIR)/strerror.o \
           $(BUILD_DIR)/time.o \
           $(BUILD_DIR)/poll.o \
           $(BUILD_DIR)/mman.o \
           $(BUILD_DIR)/stat.o \
           $(BUILD_DIR)/dirent.o \
           $(BUILD_DIR)/env.o \
           $(BUILD_DIR)/strtox.o \
           $(BUILD_DIR)/termios.o \
           $(BUILD_DIR)/auxv.o \
           $(BUILD_DIR)/runtime.o \
           $(BUILD_DIR)/dynamic_link.o \
           $(BUILD_DIR)/stevelibc_s.o \
           $(BUILD_DIR)/signal_s.o \
           $(BUILD_DIR)/threads_s.o \
           $(BUILD_DIR)/setjmp.o

LIBRARY = libstevelibc.a
TEST_DIR = test
TEST_BUILD_DIR = $(BUILD_DIR)/tests
COVERAGE_BUILD_DIR = build/coverage
COVERAGE_LIBRARY = $(COVERAGE_BUILD_DIR)/libstevelibc.a
COVERAGE_REPORT_DIR = $(COVERAGE_BUILD_DIR)/report
FUZZ_BUILD_DIR = $(BUILD_DIR)/fuzz
FUZZ_STRTOX = $(FUZZ_BUILD_DIR)/strtox
FUZZ_MEMORY_STRING = $(FUZZ_BUILD_DIR)/memory-string
FUZZ_STDIO_FORMAT = $(FUZZ_BUILD_DIR)/stdio-format
FUZZ_TARGETS = $(FUZZ_STRTOX) $(FUZZ_MEMORY_STRING) $(FUZZ_STDIO_FORMAT)
FUZZ_CORPUS_DIR = $(FUZZ_BUILD_DIR)/corpus
FUZZ_ARTIFACTS = $(FUZZ_BUILD_DIR)/artifacts
FUZZ_STRTOX_DICT ?= fuzz/dictionaries/strtox.dict
FUZZ_MEMORY_STRING_DICT ?= fuzz/dictionaries/memory_string.dict
FUZZ_STDIO_FORMAT_DICT ?= fuzz/dictionaries/stdio_format.dict
FUZZ_STRTOX_DICT_ARG = $(if $(wildcard $(FUZZ_STRTOX_DICT)),-dict=$(FUZZ_STRTOX_DICT))
FUZZ_MEMORY_STRING_DICT_ARG = $(if $(wildcard $(FUZZ_MEMORY_STRING_DICT)),-dict=$(FUZZ_MEMORY_STRING_DICT))
FUZZ_STDIO_FORMAT_DICT_ARG = $(if $(wildcard $(FUZZ_STDIO_FORMAT_DICT)),-dict=$(FUZZ_STDIO_FORMAT_DICT))
FUZZ_SECONDS ?= 15
FUZZ_ARGS ?= -max_total_time=$(FUZZ_SECONDS) -max_len=1024 -timeout=2
HEADERS = $(wildcard include/*.h include/sys/*.h)
# The sample is linked as a shared object by dynamic_link, not as an executable.
TEST_SOURCES = $(filter-out $(TEST_DIR)/dynamic_sample.c $(TEST_DIR)/coverage_support.c, \
               $(wildcard $(TEST_DIR)/*.c))
TEST_BINS = $(patsubst $(TEST_DIR)/%.c,$(TEST_BUILD_DIR)/%,$(TEST_SOURCES))
COVERAGE_CFLAGS = -ffreestanding -fno-builtin --coverage -O0 -g -Wall -Wextra -Werror \
                  -D__dso_handle=stevelibc_coverage_dso_handle \
                  -Dstdin=stevelibc_coverage_stdin -Dstdout=stevelibc_coverage_stdout \
                  -Dstderr=stevelibc_coverage_stderr -Dfopen=stevelibc_coverage_fopen \
                  -Dfclose=stevelibc_coverage_fclose -Dfflush=stevelibc_coverage_fflush \
                  -Dfseek=stevelibc_coverage_fseek -Dftell=stevelibc_coverage_ftell \
                  -Drewind=stevelibc_coverage_rewind -Dfputc=stevelibc_coverage_fputc \
                  -Dfgetc=stevelibc_coverage_fgetc -Dfread=stevelibc_coverage_fread \
                  -Dfwrite=stevelibc_coverage_fwrite -Dfputs=stevelibc_coverage_fputs \
                  -Dputs=stevelibc_coverage_puts -Dferror=stevelibc_coverage_ferror \
                  -Dfeof=stevelibc_coverage_feof -Dclearerr=stevelibc_coverage_clearerr \
                  -Dflockfile=stevelibc_coverage_flockfile \
                  -Dftrylockfile=stevelibc_coverage_ftrylockfile \
                  -Dfunlockfile=stevelibc_coverage_funlockfile -Dungetc=stevelibc_coverage_ungetc \
                  -Dsetvbuf=stevelibc_coverage_setvbuf -Dsetbuf=stevelibc_coverage_setbuf \
                  -Dgetchar=stevelibc_coverage_getchar -Dputchar=stevelibc_coverage_putchar \
                  -Dfprintf=stevelibc_coverage_fprintf -Dvfprintf=stevelibc_coverage_vfprintf \
                  -Dprintf=stevelibc_coverage_printf -Dvprintf=stevelibc_coverage_vprintf \
                  -Dsnprintf=stevelibc_coverage_snprintf -Dvsnprintf=stevelibc_coverage_vsnprintf \
                  -Dfscanf=stevelibc_coverage_fscanf -Dvfscanf=stevelibc_coverage_vfscanf \
                  -Dsscanf=stevelibc_coverage_sscanf -Dexit=stevelibc_coverage_exit \
                  -Datexit=stevelibc_coverage_atexit \
                  -D__cxa_atexit=stevelibc_coverage_cxa_atexit \
                  -D__cxa_finalize=stevelibc_coverage_cxa_finalize -Iinclude
COVERAGE_SUPPORT_OBJ = $(COVERAGE_BUILD_DIR)/coverage_support.o
COVERAGE_LIB_OBJS = $(filter-out $(COVERAGE_BUILD_DIR)/errno.o \
                                  $(COVERAGE_BUILD_DIR)/stevelibc_s.o, \
                    $(patsubst $(BUILD_DIR)/%,$(COVERAGE_BUILD_DIR)/%,$(LIB_OBJS))) \
                    $(COVERAGE_SUPPORT_OBJ)

# Ensure direct targets such as `make test` work after a clean build as well.
$(LIB_OBJS): | dirs

all: dirs $(LIBRARY) $(TARGET)

# Ensure the build directory exists before compilation
dirs:
	@mkdir -p $(BUILD_DIR)

# Compile C source files from ./src into ./build/*.o
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS)
	@echo "[CC] Compiling source: $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Explicit rules for assembling .s assembly files from ./src into ./build/*.o
$(BUILD_DIR)/auxv.o: $(SRC_DIR)/auxv.c
	@echo "[CC] Compiling source: $<"
	$(CC) $(CFLAGS) -c $< -o $@
	
$(BUILD_DIR)/stevelibc_s.o: $(SRC_DIR)/stevelibc.s
	@echo "[AS] Assembling stevelibc.s"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/signal_s.o: $(SRC_DIR)/signal.s
	@echo "[AS] Assembling signal.s"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/setjmp.o: $(SRC_DIR)/setjmp.s
	@echo "[AS] Assembling setjmp.s"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/threads_s.o: $(SRC_DIR)/threads.s
	@echo "[AS] Assembling threads.s"
	$(CC) $(CFLAGS) -c $< -o $@

# Package compiled object files into a static library archive
$(LIBRARY): $(LIB_OBJS)
	@echo "[AR] Archiving object files into static library $(LIBRARY)..."
	$(AR) rcs $@ $^

# Link user test application against our static library
$(TEST_BUILD_DIR)/%: $(TEST_DIR)/%.c $(LIBRARY) $(HEADERS)
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "[TEST-LD] Linking $<"
	$(CC) $(CFLAGS) $< $(LIBRARY) -o $@

$(TEST_BUILD_DIR)/libstevelibc_sample.so: $(TEST_DIR)/dynamic_sample.c | dirs
	@echo "[TEST-LD] Linking shared test dependency"
	$(CC) $(CFLAGS) -fPIC -shared -Wl,-soname,libstevelibc_sample.so $< -o $@

$(TEST_BUILD_DIR)/dynamic_link: $(TEST_DIR)/dynamic_link.c $(LIBRARY) $(HEADERS) \
                                $(TEST_BUILD_DIR)/libstevelibc_sample.so
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "[TEST-LD] Linking dynamic-loader test"
	$(CC) $(CFLAGS) $< $(LIBRARY) -L$(TEST_BUILD_DIR) -Wl,--no-as-needed \
		-lstevelibc_sample -Wl,-rpath,$(abspath $(TEST_BUILD_DIR)) -o $@

test: $(TEST_BINS)
	@set -e; for test_bin in $(TEST_BINS); do \
		echo "[TEST] $$test_bin"; \
		./$$test_bin; \
	done
	@echo "[SUCCESS] All standalone tests passed."

# Compile the parser separately so only the custom implementation's exported
# names are renamed; the harness can still call the host libc as an oracle.
$(FUZZ_BUILD_DIR)/strtox.o: src/strtox.c include/errno.h include/stdlib.h | dirs
	@mkdir -p $(FUZZ_BUILD_DIR)
	$(FUZZ_CC) -O1 -g -fno-omit-frame-pointer -fno-builtin \
		-fsanitize=fuzzer-no-link,address,undefined \
		-D__errno_location=stevelibc_fuzz_errno_location \
		-Dstrtoll=stevelibc_fuzz_strtoll -Dstrtoull=stevelibc_fuzz_strtoull \
		-Dstrtol=stevelibc_fuzz_strtol -Dstrtoul=stevelibc_fuzz_strtoul \
		-c $< -o $@

$(FUZZ_BUILD_DIR)/errno_shim.o: fuzz/errno_shim.c include/errno.h | dirs
	@mkdir -p $(FUZZ_BUILD_DIR)
	$(FUZZ_CC) -O1 -g -fno-omit-frame-pointer \
		-fsanitize=fuzzer-no-link,address,undefined \
		-D__errno_location=stevelibc_fuzz_errno_location \
		-c $< -o $@

$(FUZZ_STRTOX): fuzz/strtox_fuzz.c $(FUZZ_BUILD_DIR)/strtox.o \
                $(FUZZ_BUILD_DIR)/errno_shim.o
	$(FUZZ_CC) -O1 -g -fno-omit-frame-pointer -fno-builtin \
		-fsanitize=fuzzer,address,undefined \
		$(FUZZ_LDFLAGS) $< $(FUZZ_BUILD_DIR)/strtox.o \
		$(FUZZ_BUILD_DIR)/errno_shim.o -o $@

$(FUZZ_BUILD_DIR)/memory.o: src/memory.c include/string.h | dirs
	@mkdir -p $(FUZZ_BUILD_DIR)
	$(FUZZ_CC) -O1 -g -fno-omit-frame-pointer -fno-builtin \
		-fsanitize=fuzzer-no-link,address,undefined \
		-Dmemcpy=steve_memcpy -Dmemmove=steve_memmove \
		-Dmemset=steve_memset -Dmemcmp=steve_memcmp \
		-Dmemchr=steve_memchr -Dstrlen=steve_strlen \
		-Dstrnlen=steve_strnlen -Dstrcmp=steve_strcmp \
		-Dstrncmp=steve_strncmp -Dstrcpy=steve_strcpy \
		-Dstrncpy=steve_strncpy -Dstrcat=steve_strcat \
		-Dstrncat=steve_strncat -Dstrchr=steve_strchr \
		-Dstrrchr=steve_strrchr -Dstrstr=steve_strstr \
		-c $< -o $@

$(FUZZ_MEMORY_STRING): fuzz/memory_string_fuzz.c $(FUZZ_BUILD_DIR)/memory.o
	$(FUZZ_CC) -O1 -g -fno-omit-frame-pointer -fno-builtin \
		-fsanitize=fuzzer,address,undefined $(FUZZ_LDFLAGS) $^ -o $@

$(FUZZ_BUILD_DIR)/stdio.o: src/stdio.c include/stdio.h include/errno.h include/stdlib.h | dirs
	@mkdir -p $(FUZZ_BUILD_DIR)
	$(FUZZ_CC) -O1 -g -fno-omit-frame-pointer -fno-builtin \
		-ffunction-sections -fdata-sections \
		-fsanitize=fuzzer-no-link,address,undefined \
		-D__errno_location=stevelibc_fuzz_errno_location \
		-Dstdin=stevelibc_fuzz_stdin -Dstdout=stevelibc_fuzz_stdout \
		-Dstderr=stevelibc_fuzz_stderr -Dfopen=stevelibc_fuzz_fopen \
		-Dfclose=stevelibc_fuzz_fclose -Dfflush=stevelibc_fuzz_fflush \
		-Dfseek=stevelibc_fuzz_fseek -Dftell=stevelibc_fuzz_ftell \
		-Drewind=stevelibc_fuzz_rewind -Dfputc=stevelibc_fuzz_fputc \
		-Dfgetc=stevelibc_fuzz_fgetc -Dfread=stevelibc_fuzz_fread \
		-Dfwrite=stevelibc_fuzz_fwrite -Dfputs=stevelibc_fuzz_fputs \
		-Dputs=stevelibc_fuzz_puts -Dferror=stevelibc_fuzz_ferror \
		-Dfeof=stevelibc_fuzz_feof -Dclearerr=stevelibc_fuzz_clearerr \
		-Dflockfile=stevelibc_fuzz_flockfile -Dftrylockfile=stevelibc_fuzz_ftrylockfile \
		-Dfunlockfile=stevelibc_fuzz_funlockfile -Dungetc=stevelibc_fuzz_ungetc \
		-Dsetvbuf=stevelibc_fuzz_setvbuf -Dsetbuf=stevelibc_fuzz_setbuf \
		-Dgetchar=stevelibc_fuzz_getchar -Dputchar=stevelibc_fuzz_putchar \
		-Dfprintf=stevelibc_fuzz_fprintf -Dvfprintf=stevelibc_fuzz_vfprintf \
		-Dprintf=stevelibc_fuzz_printf -Dvprintf=stevelibc_fuzz_vprintf \
		-Dsnprintf=stevelibc_fuzz_snprintf -Dvsnprintf=stevelibc_fuzz_vsnprintf \
		-Dfscanf=stevelibc_fuzz_fscanf -Dvfscanf=stevelibc_fuzz_vfscanf \
		-Dsscanf=stevelibc_fuzz_sscanf \
		-c $< -o $@

$(FUZZ_STDIO_FORMAT): fuzz/stdio_format_fuzz.c $(FUZZ_BUILD_DIR)/stdio.o \
                      $(FUZZ_BUILD_DIR)/errno_shim.o
	$(FUZZ_CC) -O1 -g -fno-omit-frame-pointer -fno-builtin \
		-fsanitize=fuzzer,address,undefined $(FUZZ_LDFLAGS) \
		-Wl,--gc-sections $^ -o $@

fuzztest: $(FUZZ_TARGETS)
	@mkdir -p $(FUZZ_CORPUS_DIR)/strtox $(FUZZ_CORPUS_DIR)/memory_string \
		$(FUZZ_CORPUS_DIR)/stdio_format $(FUZZ_ARTIFACTS)
	@cp fuzz/corpus/strtox/* $(FUZZ_CORPUS_DIR)/strtox/
	@cp fuzz/corpus/memory_string/* $(FUZZ_CORPUS_DIR)/memory_string/
	@cp fuzz/corpus/stdio_format/* $(FUZZ_CORPUS_DIR)/stdio_format/
	$(FUZZ_STRTOX) $(FUZZ_CORPUS_DIR)/strtox \
		-artifact_prefix=$(FUZZ_ARTIFACTS)/ $(FUZZ_STRTOX_DICT_ARG) $(FUZZ_ARGS)
	$(FUZZ_MEMORY_STRING) $(FUZZ_CORPUS_DIR)/memory_string \
		-artifact_prefix=$(FUZZ_ARTIFACTS)/ $(FUZZ_MEMORY_STRING_DICT_ARG) $(FUZZ_ARGS)
	$(FUZZ_STDIO_FORMAT) $(FUZZ_CORPUS_DIR)/stdio_format \
		-artifact_prefix=$(FUZZ_ARTIFACTS)/ $(FUZZ_STDIO_FORMAT_DICT_ARG) $(FUZZ_ARGS)

# gcov's writer uses the hosted runtime, so coverage tests use its startup and
# errno. Exclude the custom startup/TLS objects and supply auxv test data.
coverage:
	@mkdir -p $(COVERAGE_BUILD_DIR) $(COVERAGE_REPORT_DIR)
	@rm -f $(COVERAGE_LIBRARY) $(COVERAGE_BUILD_DIR)/errno.gcno \
		$(COVERAGE_BUILD_DIR)/*.gcda $(COVERAGE_BUILD_DIR)/tests/*.gcda \
		$(COVERAGE_REPORT_DIR)/*.gcov
	$(MAKE) -B BUILD_DIR=$(COVERAGE_BUILD_DIR) LIBRARY=$(COVERAGE_LIBRARY) \
		CFLAGS='$(COVERAGE_CFLAGS)' LIB_OBJS='$(COVERAGE_LIB_OBJS)' \
		$(COVERAGE_SUPPORT_OBJ) test
	find $(COVERAGE_BUILD_DIR) -type f -name '*.gcda' -size 0 -delete
	gcov -b -c -o $(COVERAGE_BUILD_DIR) \
		$(filter-out src/errno.c,$(wildcard src/*.c))
	@for report in $(patsubst src/%.c,%.c.gcov,$(filter-out src/errno.c,$(wildcard src/*.c))) syscall.h.gcov; do \
		if test -f "$$report"; then mv "$$report" "$(COVERAGE_REPORT_DIR)/"; fi; \
	done
	@echo "[SUCCESS] Coverage report written to $(COVERAGE_REPORT_DIR)/."

$(COVERAGE_SUPPORT_OBJ): test/coverage_support.c include/sys/auxv.h | dirs
	@mkdir -p $(COVERAGE_BUILD_DIR)
	$(CC) $(COVERAGE_CFLAGS) -c $< -o $@

clean:
	@echo "[CLEAN] Purging build artifacts and object directory..."
	rm -rf $(BUILD_DIR) $(LIBRARY) $(TARGET)

.PHONY: all dirs clean test fuzztest coverage