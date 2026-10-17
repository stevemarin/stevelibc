# ==============================================================================
# THREADS.S : Linux clone syscall and child-thread trampoline
# ==============================================================================

.global __clone_thread
.type __clone_thread, @function
__clone_thread:
    # SysV arguments: flags, child_stack, ptid, ctid, tls, child_arg.
    # Linux x86_64 clone arguments 4 and 5 are in r10 and r8.
    movq %rcx, %r10
    movq $56, %rax
    syscall

    testq %rax, %rax
    jnz .L_clone_return

    # The kernel starts the child at this point with the new stack and TLS.
    movq %r9, %rdi
    movq %rdi, %r12             # Keep the control record across the C callback.
    xorq %rbp, %rbp
    andq $-16, %rsp
    call thread_wrapper

    # Record the user result and ask C whether this detached thread can self-reap.
    movl %eax, %r14d
    movq %r12, %rdi
    movl %r14d, %esi
    call __thread_complete

    # The two-register result carries stack address and size for detached cleanup.
    movq %rax, %r12
    movq %rdx, %r13
    testq %r12, %r12
    jz .L_thread_exit

    # After munmap returns, do not touch the now-unmapped child stack again.
    movq %r12, %rdi
    movq %r13, %rsi
    movq $11, %rax              # SYS_munmap
    syscall

.L_thread_exit:
    # A CLONE_THREAD child exits independently; SYS_exit_group is not wanted.
    movl %r14d, %edi
    movq $60, %rax
    syscall
    ud2

.L_clone_return:
    ret
# The parent resumes with the clone syscall result in %rax.
