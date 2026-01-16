# Test a regression related to two static libs that have the same filename in it, with local symbols
# The looking for local symbol tables was based on filename alone instead of a combination of libname and filename.

.section .text
.globl _start

_start:
    call f1             # eax=5
    mov %eax, %ebx      # ebx is the exit code
    call f2             # eax=4
    add %eax, %ebx      # eax=9
    sub $9, %ebx        # Should be zero
    movl $1, %eax       # put sys_exit code into %eax for the syscall
    int $0x80           # Call kernel sys_exit
