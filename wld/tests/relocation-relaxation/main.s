# Test relocation relaxation based on what's in crt1.o and crti.o
# These would normally be linked in with libc

.section .text
.globl main

# Test linker opcode rewriting. The instructions are changed by the linker to not use a GOT.
relaxed_relocation_testing:
    # Test R_X86_64_GOTPCREL aka R_X86_64_GOTPCRELX
    movl reg00@GOTPCREL(%rip), %eax
    movq (%rax), %rax ; cmp $100, %rax
    jne not_ok

    # Test R_X86_64_REX_GOTPCRELX
    movq reg00@GOTPCREL(%rip),  %rax ; movq (%rax), %rax ; cmp $100, %rax ; jne not_ok
    movq reg01@GOTPCREL(%rip),  %rcx ; movq (%rcx), %rax ; cmp $101, %rax ; jne not_ok
    movq reg02@GOTPCREL(%rip),  %rdx ; movq (%rdx), %rax ; cmp $102, %rax ; jne not_ok
    movq reg03@GOTPCREL(%rip),  %rbx ; movq (%rbx), %rax ; cmp $103, %rax ; jne not_ok
    movq reg06@GOTPCREL(%rip),  %rsi ; movq (%rsi), %rax ; cmp $106, %rax ; jne not_ok
    movq reg07@GOTPCREL(%rip),  %rdi ; movq (%rdi), %rax ; cmp $107, %rax ; jne not_ok
    movq reg08@GOTPCREL(%rip),  %r8  ; movq (%r8 ), %rax ; cmp $108, %rax ; jne not_ok
    movq reg09@GOTPCREL(%rip),  %r9  ; movq (%r9 ), %rax ; cmp $109, %rax ; jne not_ok
    movq reg10@GOTPCREL(%rip),  %r10 ; movq (%r10), %rax ; cmp $110, %rax ; jne not_ok
    movq reg11@GOTPCREL(%rip),  %r11 ; movq (%r11), %rax ; cmp $111, %rax ; jne not_ok
    movq reg12@GOTPCREL(%rip),  %r12 ; movq (%r12), %rax ; cmp $112, %rax ; jne not_ok
    movq reg13@GOTPCREL(%rip),  %r13 ; movq (%r13), %rax ; cmp $113, %rax ; jne not_ok
    movq reg14@GOTPCREL(%rip),  %r14 ; movq (%r14), %rax ; cmp $114, %rax ; jne not_ok
    movq reg15@GOTPCREL(%rip),  %r15 ; movq (%r15), %rax ; cmp $115, %rax ; jne not_ok

    # Special case for stack registers
    mov %rsp, %rbx # Make backups
    mov %rbp, %rcx

    movq reg04@GOTPCREL(%rip),  %rsp ; movq (%rsp), %rax ; cmp $104, %rax ; jne stack_not_ok
    movq reg05@GOTPCREL(%rip),  %rbp ; movq (%rbp), %rax ; cmp $105, %rax ; jne stack_not_ok

    jmp stack_ok

not_ok:
    movl $1, %eax;
    ret

stack_not_ok:
    movl $1, %eax;
    mov %rbx, %rsp  # Restore the stack registers
    mov %rcx, %rbp
    ret

stack_ok:
    movl $0, %eax;
    mov %rbx, %rsp  # Restore the stack registers
    mov %rcx, %rbp
    ret

main:
    call relaxed_relocation_testing
    addl $42, %eax
    ret

.section .data
reg00: .quad 100
reg01: .quad 101
reg02: .quad 102
reg03: .quad 103
reg04: .quad 104
reg05: .quad 105
reg06: .quad 106
reg07: .quad 107
reg08: .quad 108
reg09: .quad 109
reg10: .quad 110
reg11: .quad 111
reg12: .quad 112
reg13: .quad 113
reg14: .quad 114
reg15: .quad 115
