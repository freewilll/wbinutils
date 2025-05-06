# This tests a secton with an odd name. The linker is expected to include it.
# Two object files are included to test the merging of the .oddball sections.

.text
.globl _start

_start:
    movl $1, %eax       # sys_exit
    movl i(%rip), %ebx
    movl j(%rip), %ecx
    add %ecx, %ebx      # exit code
    int $0x80           # Call kernel

.section .oddball, "aw"
i: .long 0
