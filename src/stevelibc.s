# ==============================================================================
# MINILIBC.S : Naked Entry Point, Stack Argument & Auxv Parser
# ==============================================================================

.global _start
.type _start, @function
.weak __preinit_array_start
.weak __preinit_array_end
.weak __init_array_start
.weak __init_array_end
_start:
    xorq %rbp, %rbp             # Clear frame pointer for stack unwinding safety

    # Install the main thread's TLS block before entering any C code.
    movq $158, %rax             # SYS_arch_prctl
    movq $0x1002, %rdi          # ARCH_SET_FS
    leaq __main_tcb(%rip), %rsi
    syscall
    testq %rax, %rax
    js .L_tls_setup_failed

    movq (%rsp), %r12           # Preserve argc across constructor calls
    leaq 8(%rsp), %r13          # Preserve argv across constructor calls
    movq %r13, %rsi

    # Scan past argv array to locate environment variables pointer (envp)
    movq %rsi, %rax
.L_scan_argv:
    lodsq
    testq %rax, %rax
    jnz .L_scan_argv
    # Now %rsi points to envp[0]
    movq %rsi, %r14             # Preserve envp across constructor calls

    # Save environment pointer globally
    movq %r14, environ(%rip)

    # Scan past envp array to locate auxiliary vector (auxv)
    movq %rsi, %rax
.L_scan_envp:
    lodsq
    testq %rax, %rax
    jnz .L_scan_envp
    # Now %rsi points to the auxiliary vector (Elf64_auxv_t array)

    # Save auxv pointer globally
    movq %rsi, __auxv_ptr(%rip)

    # The ELF interpreter has already mapped dependencies and applied dynamic
    # relocations before transferring control to this executable's entry point.
    # This runtime then handles the main image's preinit/init arrays; exit()
    # later walks the fini array after registered callbacks.
    #
    # Weak array boundaries resolve to empty ranges when a program has no array.
    # Preinitializers must run before constructors, and both run before main.
    leaq __preinit_array_start(%rip), %rbx
    leaq __preinit_array_end(%rip), %r15
.L_preinit_loop:
    cmpq %r15, %rbx
    jae .L_init_array_begin
    movq (%rbx), %rax
    # Advance before calling: callbacks may alter caller-saved registers.
    addq $8, %rbx
    testq %rax, %rax
    jz .L_preinit_loop
    call *%rax
    jmp .L_preinit_loop

.L_init_array_begin:
    # Constructors execute in link order before main.
    leaq __init_array_start(%rip), %rbx
    leaq __init_array_end(%rip), %r15
.L_init_array_loop:
    cmpq %r15, %rbx
    jae .L_call_main
    movq (%rbx), %rax
    addq $8, %rbx
    testq %rax, %rax
    jz .L_init_array_loop
    call *%rax
    jmp .L_init_array_loop

.L_call_main:
    # Rebuild main's three arguments after constructor calls.
    movq %r12, %rdi
    movq %r13, %rsi
    movq %r14, %rdx
    call main

    movq %rax, %rdi
    call exit

.L_tls_setup_failed:
    movq $60, %rax              # SYS_exit
    movq $127, %rdi
    syscall

.bss
.globl environ
.align 8
environ:
    .quad 0

.globl __auxv_ptr
.align 8
__auxv_ptr:
    .quad 0
