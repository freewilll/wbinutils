# Test two files, each with a local symbol of the same name

.section .text
.globl _start

_start:
    movq data@GOTPCREL(%rip),  %rax
    movq (%rax), %rax
    cmp $1, %rax
    jne not_ok

    call f1
    cmp $2, %rax
    jne not_ok

    call f2
    cmp $3, %rax
    jne not_ok

    # Test reading local symbols in two different files
    call f1l
    cmp $10, %rax
    jne not_ok

    call f2l
    cmp $20, %rax
    jne not_ok

ok:
    movl $0, %ebx
    movl $1, %eax
    int $0x80

not_ok:
    movl $1, %ebx
    movl $1, %eax
    int $0x80

.section .data
data: .quad 1
