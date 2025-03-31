# Two source files, with relocations in both pointing to the other.
# It exits with a magic code.

.section .text
.globl _start
.globl i

_start:
    # Call sys_exit(int status)
    call f              # f is present in the other file
    mov %eax, %ebx      # ebx is the exit code
    movl $1, %eax       # put sys_exit code into %eax for the syscall
    addl j, %ebx;       # j is present in the other file
    addl k, %ebx;       # k is present in the other file
    int $0x80           # Call kernel

.section .data
i: .byte 2              # Read by the other file
