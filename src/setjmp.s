# ==============================================================================
# SETJMP.S : Non-Local CPU Register Context Switching
# ------------------------------------------------------------------------------
# setjmp saves all callee-saved x86_64 registers and instruction return pointer
# into a jump buffer (jmp_buf). longjmp restores them, jumping back to the 
# setjmp call site instantly without unwinding normal function return frames.
# ==============================================================================

.global setjmp
.type setjmp, @function
setjmp:
    # rdi holds pointer to jmp_buf array
    movq %rbx, 0(%rdi)       # Save rbx
    movq %rbp, 8(%rdi)       # Save rbp
    movq %r12, 16(%rdi)      # Save r12
    movq %r13, 24(%rdi)      # Save r13
    movq %r14, 32(%rdi)      # Save r14
    movq %r15, 40(%rdi)      # Save r15
    
    leaq 8(%rsp), %rax       # Calculate original stack pointer before call instruction
    movq %rax, 48(%rdi)      # Save rsp
    
    movq (%rsp), %rax        # Extract return address (instruction pointer) from stack top
    movq %rax, 56(%rdi)      # Save rip
    
    xorq %rax, %rax          # Return 0 on initial setjmp invocation
    ret

.global longjmp
.type longjmp, @function
longjmp:
    # rdi holds pointer to jmp_buf, rsi holds return value override (val)
    movq 0(%rdi), %rbx       # Restore rbx
    movq 8(%rdi), %rbp       # Restore rbp
    movq 16(%rdi), %r12      # Restore r12
    movq 24(%rdi), %r13      # Restore r13
    movq 32(%rdi), %r14      # Restore r14
    movq 40(%rdi), %r15      # Restore r15
    movq 48(%rdi), %rsp      # Restore stack pointer
    
    movq %rsi, %rax          # Move return value into %rax
    testq %rax, %rax
    jnz 1f
    movq $1, %rax            # POSIX rule: if longjmp val is 0, coerce to 1
1:
    movq 56(%rdi), %r10      # Load target instruction pointer into scratch register
    jmp *%r10                # Jump directly back to setjmp execution site
