static int fuzz_errno;

int *__errno_location(void) { return &fuzz_errno; }
