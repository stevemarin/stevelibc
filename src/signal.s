# ==============================================================================
# SIGNAL.S : Kernel Signal Return Trampoline
# ------------------------------------------------------------------------------
# When a signal handler finishes execution, it must invoke SYS_rt_sigreturn (15)
# so the kernel can restore the interrupted register state.
# ==============================================================================

.global __restore_rt
.type __restore_rt, @function
__restore_rt:
    movq $15, %rax              # SYS_rt_sigreturn system call number
    syscall                     # Kernel mode transition to resume execution
