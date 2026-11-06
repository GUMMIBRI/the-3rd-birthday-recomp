/* SPDX-License-Identifier: GPL-2.0-or-later
 * SysV call boundary: caller-saved GPR/SIMD registers belong to the compiler.
 * Preserve callee-saved GPRs and per-context FP control state explicitly.
 */
.text
.globl sr_sysv_swap
.type sr_sysv_swap,@function
sr_sysv_swap:
    movq %rsp, 0(%rdi)
    movq %rbx, 8(%rdi)
    movq %rbp, 16(%rdi)
    movq %r12, 24(%rdi)
    movq %r13, 32(%rdi)
    movq %r14, 40(%rdi)
    movq %r15, 48(%rdi)
    stmxcsr 56(%rdi)
    fnstcw 60(%rdi)
    movq 8(%rsi), %rbx
    movq 16(%rsi), %rbp
    movq 24(%rsi), %r12
    movq 32(%rsi), %r13
    movq 40(%rsi), %r14
    movq 48(%rsi), %r15
    ldmxcsr 56(%rsi)
    fldcw 60(%rsi)
    movq 0(%rsi), %rsp
    ret
.size sr_sysv_swap, .-sr_sysv_swap
.section .note.GNU-stack,"",@progbits
