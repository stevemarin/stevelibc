# stevelibc

`stevelibc` is a small x86_64 Linux libc written as a teaching project. It
shows how ordinary C interfaces sit on top of the Linux syscall ABI, how a
process starts before `main`, and how libc adds policy and state around
kernel operations.

This is intentionally **not** a drop-in replacement for glibc or a complete
POSIX implementation. Its headers describe this project's supported subset,
some structures are specific to its ABI, and many standard interfaces and
edge cases are not implemented. Use it to study mechanisms, not to link
arbitrary Linux programs.

## A useful reading order

1. **Program entry:** start at [`src/stevelibc.s`](src/stevelibc.s). Linux
   enters the executable at `_start`, not `main`; startup establishes TLS,
   walks the initial stack to find `argv`, `environ`, and auxv, then runs
   preinitializers and constructors before calling `main`. Normal `exit`
   runs `atexit`/C++ ABI callbacks and loaded-object fini arrays before
   flushing streams; `_exit` deliberately bypasses that work. Normal `exit`
   runs C/C++ termination callbacks and the ELF fini array before flushing
   streams; `_exit` deliberately bypasses this cleanup.
2. **Syscalls and errors:** read [`include/syscall.h`](include/syscall.h)
   alongside [`src/stevelibc.c`](src/stevelibc.c). The assembly `syscall`
   instruction uses a Linux-specific register convention; wrappers translate
   kernel negative error numbers into `-1` plus thread-local `errno`.
   [`src/time.c`](src/time.c) is a compact example of passing an ABI-defined
   structure directly to Linux for monotonic/realtime clock reads, legacy
   microsecond wall time through [`include/sys/time.h`](include/sys/time.h),
   and relative thread sleeps.
   [`include/poll.h`](include/poll.h) and [`src/poll.c`](src/poll.c) show how
   `poll` waits atomically for readiness on multiple descriptors; the tests use
   pipes to demonstrate readable, writable, ignored, and invalid descriptors.
3. **Memory and strings:** read [`src/memory.c`](src/memory.c) and
   [`src/malloc.c`](src/malloc.c). The former implements familiar byte-level
   operations, `bsearch` over sorted arrays, signed integer utilities, string
   duplication, and character-set span searches; `fgets` provides bounded line
   input through the buffered stream layer. The latter manages aligned blocks
   over the process break and protects its shared metadata with a lock.
4. **Files and directories:** follow the public declarations in
   [`include/unistd.h`](include/unistd.h), [`include/fcntl.h`](include/fcntl.h),
   [`include/sys/stat.h`](include/sys/stat.h), and
   [`include/dirent.h`](include/dirent.h) into
   [`src/stat.c`](src/stat.c) and [`src/dirent.c`](src/dirent.c). Notice that
   Linux's `getdents64` records are parsed into a separate public
   `struct dirent`; kernel record layout and C API layout are not interchangeable.
   The descriptor-relative `*at` calls are useful when a directory fd should
   anchor name resolution; file-descriptor operations such as `fsync`,
   `ftruncate`, and `fchmod` avoid resolving the pathname again.
5. **Process environment and numbers:** read [`src/env.c`](src/env.c) to see
   how startup's borrowed environment is copied before mutation, and
   [`src/strtox.c`](src/strtox.c) for base selection, end pointers, and
   overflow detection.
6. **Tests:** run `make test`. Tests are standalone programs linked against
   this library; [`test/api_surface.c`](test/api_surface.c) checks representative
   public APIs and ABI layout assumptions.
   Run `make sanitize` for the AddressSanitizer and UndefinedBehaviorSanitizer
   checks. These instrumented runs use isolated hosted fuzz harnesses rather
   than the standalone executables: the library's freestanding startup installs
   its own TLS block, which is not compatible with sanitizer runtimes.
   For process creation, compare `fork`, `execve`/`execv`/`execvp`, and
   `waitpid` in [`src/stevelibc.c`](src/stevelibc.c). `execvp` searches PATH
   and preserves permission failures, but this teaching subset does not
   implement the optional shell fallback for `ENOEXEC`.
   The system ELF interpreter relocates dynamic executables before `_start`;
   [`src/dynamic_link.c`](src/dynamic_link.c) uses the loader's `DT_DEBUG`
   rendezvous to expose loaded program headers through `dl_iterate_phdr` and
   to call dependency finalizers in reverse load order during normal exit.
   This is runtime integration, not a dynamic loader: `dlopen`, `dlsym`, and
   `dlclose` are not provided, and iteration/finalization do not take the
   interpreter's private lock. The implementation relies on the GNU-compatible
   x86_64 `r_debug`/`link_map` prefix.
7. **Signals and terminals:** [`src/signal.c`](src/signal.c) wraps Linux's
   real-time signal syscalls, provides bit-set helpers, and installs the
   assembly `rt_sigreturn` trampoline for C handlers. Signal masks are per
   thread; signal sets use the x86_64 kernel's 64-bit representation.
   [`src/termios.c`](src/termios.c) translates between the public 32-character
   `struct termios` and the kernel's shorter `TCGETS`/`TCSETS` layout (one line
   discipline byte and 19 control characters). It also implements terminal
   queue, flow, foreground-process-group, speed, and raw-mode helpers. The
   pseudoterminal checks in [`test/signal_termios.c`](test/signal_termios.c)
   exercise real kernel state changes and signal delivery.
8. **Fuzzing:** `make fuzztest` builds separate Clang/libFuzzer targets for
   integer parsing, memory/string operations, and formatted string input/output.
   Differential targets compare results with the host libc while
   AddressSanitizer and UndefinedBehaviorSanitizer check the code under test.
   This deliberately focuses on deterministic routines; syscalls, thread
   lifecycle, signals, and other kernel-stateful behavior are better covered
   by controlled integration tests. Each target runs for `FUZZ_SECONDS`
   seconds (15 by default). The targets store their growing corpora and crash
   artifacts under `build/fuzz/`; for example, run each for two minutes with
   `make fuzztest FUZZ_SECONDS=120`.

## Design choices worth noticing

- The project targets the **Linux x86_64 syscall ABI**, not a generic Unix
  syscall interface. Syscall numbers, register assignments, `struct stat`,
  and the startup stack interpretation all depend on that target.
- Public headers define target-specific types and constants. This makes the
  ABI assumptions inspectable, but means these headers should not be mixed
  indiscriminately with another libc's headers in one translation unit.
- The kernel reports syscall errors as negative values in a reserved range.
  Keeping conversion in one helper makes wrappers consistent and gives
  `errno` its usual per-thread behavior. `strerror` maps the supported error
  numbers to static descriptions, and `perror` writes the current description
  to `stderr` without replacing the error being reported.
- A libc is more than syscall stubs. It also owns buffering, allocation,
  environment mutation, process startup, and conversion of kernel data into
  public interfaces. Those layers are deliberately visible here.
- Simplicity has limits. For example, the current allocator uses `brk`, the
  environment API has a small implementation-specific concurrency model, and
  formatted I/O and signal support cover subsets only. Character classification
  in [`include/ctype.h`](include/ctype.h) is ASCII-only and locale-independent.
  Read the code and tests as the authoritative statement of what is supported.
- Stdio provides bounded `snprintf`, common formatted output and input
  conversions, scansets, and ASCII-only wide-character conversions. Decimal
  floating output is a teaching implementation with a capped precision, not
  a correctly-rounded general-purpose conversion algorithm. `vfscanf` currently
  buffers the remaining contents of a seekable stream, so it is not suitable
  for pipes or arbitrarily large inputs. Each `FILE` has a recursive lock;
  `flockfile`, `ftrylockfile`, and `funlockfile` expose that lock for callers
  that need a multi-call critical section.

## Further reading

These references explain the external contracts and architecture behind the
implementation:

- [Linux man-pages: `syscall(2)`](https://man7.org/linux/man-pages/man2/syscall.2.html) —
  syscall calling conventions, including architecture-specific details.
- [Linux man-pages: `syscalls(2)`](https://man7.org/linux/man-pages/man2/syscalls.2.html) —
  Linux system-call overview and links to individual calls.
- [Linux man-pages: `openat(2)`](https://man7.org/linux/man-pages/man2/openat.2.html) —
  directory-relative path operations and descriptor-based APIs.
- [Linux man-pages: `getdents(2)`](https://man7.org/linux/man-pages/man2/getdents.2.html) —
  kernel directory-entry records returned by Linux.
- [Linux man-pages: `stat(2)`](https://man7.org/linux/man-pages/man2/stat.2.html) —
  file metadata interfaces and their semantics.
- [Linux man-pages: `wait(2)`](https://man7.org/linux/man-pages/man2/wait.2.html) —
  waiting for child processes and interpreting status values.
- [Linux man-pages: `strtol(3)`](https://man7.org/linux/man-pages/man3/strtol.3.html) —
  integer conversion behavior, bases, end pointers, and range errors.
- [Linux man-pages: `getenv(3)`](https://man7.org/linux/man-pages/man3/getenv.3.html) and
  [`environ(7)`](https://man7.org/linux/man-pages/man7/environ.7.html) —
  process environment conventions.
- [Linux man-pages: `brk(2)`](https://man7.org/linux/man-pages/man2/brk.2.html) —
  the process-break interface used by this teaching allocator.
- [Linux man-pages: `mmap(2)`](https://man7.org/linux/man-pages/man2/mmap.2.html) —
  virtual-memory mappings, used here for thread stacks.
- [Linux man-pages: `getauxval(3)`](https://man7.org/linux/man-pages/man3/getauxval.3.html) and
  [`elf(5)`](https://man7.org/linux/man-pages/man5/elf.5.html) —
  auxiliary-vector values and executable format background.
- [Linux man-pages: `dl_iterate_phdr(3)`](https://man7.org/linux/man-pages/man3/dl_iterate_phdr.3.html) —
  enumerating loaded ELF objects through their program headers.
- [Itanium C++ ABI: initialization and termination](https://itanium-cxx-abi.github.io/cxx-abi/abi.html#dso-dtor) —
  the `__cxa_atexit` and `__cxa_finalize` hooks used for C++ object lifetime.
- [Linux man-pages: `pthread_create(3)`](https://man7.org/linux/man-pages/man3/pthread_create.3.html) —
  a higher-level thread API to compare with this project's deliberately small
  clone-based thread functions.
- [Linux man-pages: `errno(3)`](https://man7.org/linux/man-pages/man3/errno.3.html) —
  why `errno` is exposed as a per-thread accessor rather than a simple global.
- [Linux man-pages: `signal(7)`](https://man7.org/linux/man-pages/man7/signal.7.html),
  [`sigaction(2)`](https://man7.org/linux/man-pages/man2/sigaction.2.html), and
  [`sigreturn(2)`](https://man7.org/linux/man-pages/man2/sigreturn.2.html) —
  signal delivery, dispositions, masks, and restoration of interrupted CPU state.
- [Linux man-pages: `termios(3)`](https://man7.org/linux/man-pages/man3/termios.3.html)
  and [`ioctl_tty(2)`](https://man7.org/linux/man-pages/man2/ioctl_tty.2.html) —
  terminal modes, control characters, and the kernel ioctl interface.
- [System V ABI: AMD64 Architecture Processor Supplement](https://gitlab.com/x86-psABIs/x86-64-ABI) —
  x86_64 process and function ABI background.
- [The Open Group Base Specifications, Issue 8](https://pubs.opengroup.org/onlinepubs/9799919799/) —
  portable POSIX interface specifications. This project implements only a
  subset, and Linux-specific behavior may differ.

### Running the fuzzer

Install Clang with its libFuzzer and sanitizer runtime support, then run
`make fuzztest`. On Debian/Ubuntu, install `clang` and the matching
`libclang-rt-<version>-dev` package (some distributions bundle the runtime with
Clang). No package is needed for the normal `make test` build. Findings are
written under `build/fuzz/artifacts/`; retain them as regression inputs in the
standalone test suite before changing library behavior. Optional per-target
libFuzzer dictionaries are picked up from `fuzz/dictionaries/` when present:
`strtox.dict`, `memory_string.dict`, and `stdio_format.dict`. Override their
paths with `FUZZ_STRTOX_DICT`, `FUZZ_MEMORY_STRING_DICT`, or
`FUZZ_STDIO_FORMAT_DICT`; set one to an empty value to disable that dictionary.
When copying libFuzzer's recommended dictionary, omit the trailing `# Uses:`
annotations; dictionary tokens should use supported escapes such as `\xNN`.

## Build and test

```sh
make
make test
make coverage
```

`make coverage` rebuilds and runs the standalone test suite with GCC's
`--coverage` instrumentation, then writes line and branch reports under
`build/coverage/report/`. The report covers instrumented C sources in `src/`
(the project-specific errno/TLS accessor is excluded); GCC and its matching
`gcov` utility are required. Coverage executables use the host startup/runtime
so gcov can write its data, so use `make test` for the actual freestanding
startup and TLS checks.
