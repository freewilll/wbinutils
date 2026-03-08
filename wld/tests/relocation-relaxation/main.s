# Test relocation relaxation based on what's in crt1.o and crti.o
# These would normally be linked in with libc

.section .text
.globl main

exit_with_not_ok:
    movl $1, %eax;
    ret

# Test linker opcode rewriting.
# In the static case, the instructions are changed by the linker to not use a GOT.
# In the dynamic case, instrutions are relaxed for GOTPCRELX relocations, or
# GOT entries + relocations are added for GOTPCREL.
main:
    # Test R_X86_64_GOTPCREL* relocations
    movq reg00@GOTPCREL(%rip), %rax
    movq (%rax), %rax ; cmp $100, %rax
    jne exit_with_not_ok

    movq reg00@GOTPCREL(%rip), %rcx
    movq (%rcx), %rax ; cmp $100, %rax
    jne exit_with_not_ok

    # Test R_X86_64_REX_GOTPCREL*
    movq reg00@GOTPCREL(%rip),  %rax ; movq (%rax), %rax ; cmp $100, %rax ; jne exit_with_not_ok
    movq reg01@GOTPCREL(%rip),  %rcx ; movq (%rcx), %rax ; cmp $101, %rax ; jne exit_with_not_ok
    movq reg02@GOTPCREL(%rip),  %rdx ; movq (%rdx), %rax ; cmp $102, %rax ; jne exit_with_not_ok
    movq reg03@GOTPCREL(%rip),  %rbx ; movq (%rbx), %rax ; cmp $103, %rax ; jne exit_with_not_ok
    movq reg06@GOTPCREL(%rip),  %rsi ; movq (%rsi), %rax ; cmp $106, %rax ; jne exit_with_not_ok
    movq reg07@GOTPCREL(%rip),  %rdi ; movq (%rdi), %rax ; cmp $107, %rax ; jne exit_with_not_ok
    movq reg08@GOTPCREL(%rip),  %r8  ; movq (%r8),  %rax ; cmp $108, %rax ; jne exit_with_not_ok
    movq reg09@GOTPCREL(%rip),  %r9  ; movq (%r9),  %rax ; cmp $109, %rax ; jne exit_with_not_ok
    movq reg10@GOTPCREL(%rip),  %r10 ; movq (%r10), %rax ; cmp $110, %rax ; jne exit_with_not_ok
    movq reg11@GOTPCREL(%rip),  %r11 ; movq (%r11), %rax ; cmp $111, %rax ; jne exit_with_not_ok
    movq reg12@GOTPCREL(%rip),  %r12 ; movq (%r12), %rax ; cmp $112, %rax ; jne exit_with_not_ok
    movq reg13@GOTPCREL(%rip),  %r13 ; movq (%r13), %rax ; cmp $113, %rax ; jne exit_with_not_ok
    movq reg14@GOTPCREL(%rip),  %r14 ; movq (%r14), %rax ; cmp $114, %rax ; jne exit_with_not_ok
    movq reg15@GOTPCREL(%rip),  %r15 ; movq (%r15), %rax ; cmp $115, %rax ; jne exit_with_not_ok

    # Special case for stack registers
    mov %rsp, %rbx # Make backups
    mov %rbp, %rcx

    movq reg04@GOTPCREL(%rip),  %rsp ; movq (%rsp), %rax ; cmp $104, %rax ; jne restore_stack_and_exit_with_not_ok
    movq reg05@GOTPCREL(%rip),  %rbp ; movq (%rbp), %rax ; cmp $105, %rax ; jne restore_stack_and_exit_with_not_ok

    jmp stack_ok

restore_stack_and_exit_with_not_ok:
    mov %rbx, %rsp  # Restore the stack registers
    mov %rcx, %rbp
    jmp exit_with_not_ok

stack_ok:
    mov %rbx, %rsp  # Restore the stack registers
    mov %rcx, %rbp

    # Check compare instructions
    # Not doing rsp and rbp for convenience. They aren't special cases in the encoding anyways.
    lea reg00(%rip), %rax ; cmpq reg00@GOTPCREL(%rip), %rax ; jne exit_with_not_ok
    lea reg01(%rip), %rcx ; cmpq reg01@GOTPCREL(%rip), %rcx ; jne exit_with_not_ok
    lea reg02(%rip), %rdx ; cmpq reg02@GOTPCREL(%rip), %rdx ; jne exit_with_not_ok
    lea reg03(%rip), %rbx ; cmpq reg03@GOTPCREL(%rip), %rbx ; jne exit_with_not_ok
    lea reg06(%rip), %rsi ; cmpq reg06@GOTPCREL(%rip), %rsi ; jne exit_with_not_ok
    lea reg07(%rip), %rdi ; cmpq reg07@GOTPCREL(%rip), %rdi ; jne exit_with_not_ok
    lea reg08(%rip), %r8  ; cmpq reg08@GOTPCREL(%rip), %r8  ; jne exit_with_not_ok
    lea reg09(%rip), %r9  ; cmpq reg09@GOTPCREL(%rip), %r9  ; jne exit_with_not_ok
    lea reg10(%rip), %r10 ; cmpq reg10@GOTPCREL(%rip), %r10 ; jne exit_with_not_ok
    lea reg11(%rip), %r11 ; cmpq reg11@GOTPCREL(%rip), %r11 ; jne exit_with_not_ok
    lea reg12(%rip), %r12 ; cmpq reg12@GOTPCREL(%rip), %r12 ; jne exit_with_not_ok
    lea reg13(%rip), %r13 ; cmpq reg13@GOTPCREL(%rip), %r13 ; jne exit_with_not_ok
    lea reg14(%rip), %r14 ; cmpq reg14@GOTPCREL(%rip), %r14 ; jne exit_with_not_ok
    lea reg15(%rip), %r15 ; cmpq reg15@GOTPCREL(%rip), %r15 ; jne exit_with_not_ok

    # A couple of more instructions
    lea reg00(%rip), %rax ; neg %rax;                         addq reg00@GOTPCREL(%rip), %rax ;                                   jnz exit_with_not_ok
    lea reg00(%rip), %rax ; neg %rax;           clc;          adcq reg00@GOTPCREL(%rip), %rax ;                                   jnz exit_with_not_ok
    lea reg00(%rip), %rax ; neg %rax; dec %rax; stc;          adcq reg00@GOTPCREL(%rip), %rax ;                                   jnz exit_with_not_ok
    lea reg00(%rip), %rax ;                                   subq reg00@GOTPCREL(%rip), %rax ;                                   jnz exit_with_not_ok
    lea reg00(%rip), %rax ;                     clc;          sbbq reg00@GOTPCREL(%rip), %rax ;                                   jnz exit_with_not_ok
    lea reg00(%rip), %rax ;           inc %rax; stc;          sbbq reg00@GOTPCREL(%rip), %rax ;                                   jnz exit_with_not_ok
    lea reg00(%rip), %rax;                                    xorq reg00@GOTPCREL(%rip), %rax ;                                   jnz exit_with_not_ok
    movq $-1, %rax;                                           andq reg00@GOTPCREL(%rip), %rax ; cmpq reg00@GOTPCREL(%rip), %rax ; jne exit_with_not_ok
    movq $0, %rax ;                                           orq  reg00@GOTPCREL(%rip), %rax ; cmpq reg00@GOTPCREL(%rip), %rax ; jne exit_with_not_ok

    .extern external_data
    movq external_data@GOTPCREL(%rip), %rax
    movq (%rax), %rax
    cmp $200, %rax
    jne exit_with_not_ok

    # Check callq *func(%rip)
    call *func@GOTPCREL(%rip)
    cmp $202, %rax
    jne exit_with_not_ok

    # Check jmp *func(%rip)
    jmp *jmp@GOTPCREL(%rip)
    jmp exit_with_not_ok
jmp:

    # Check a GOTPCREL for an undefined weak symbol works as it should
    .weak weak_fn        # undefined weak symbol
    movq weak_fn@GOTPCREL(%rip), %rax
    test %rax, %rax
    jne exit_with_not_ok

    movl $0, %eax;
    ret

func:
    movq $202, %rax
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
