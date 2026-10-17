#ifndef STEVELIBC_TLS_H
#define STEVELIBC_TLS_H

typedef struct tls_control_block {
  /*
   * This is the small thread-local data block used by the runtime. Assembly
   * installs its address in the x86_64 FS base; self is at offset zero because
   * the compiler-generated TLS access sequence expects that anchor.
   */
  struct tls_control_block *self;
  int errno_value; /* Per-thread errno storage returned by __errno_location */
} tls_control_block;

/* Initial thread's TCB; newly created threads receive separately allocated ones. */
extern tls_control_block __main_tcb;

#endif
