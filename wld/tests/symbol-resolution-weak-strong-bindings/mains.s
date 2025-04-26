.section .text
.globl _start

_start:
    call f
    mov %eax, %ebx      # ebx is the exit code
    movl $1, %eax       # put sys_exit code into %eax for the syscall
    int $0x80           # Call kernel sys_exit
