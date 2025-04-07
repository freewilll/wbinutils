# Test a relocation with a local symbol. The relocation references the .data section.

.text
.globl _start

_start:
    movl $1, %eax       # sys_exit
    movl i, %ebx        # exit code
    int $0x80           # Call kernel

.section .data
    .align   4
    .type    i, @object
    .size    i, 4
i:
    .long    42
