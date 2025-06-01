.section .text
.globl _start

_start:
    call g
    call h
    mov %eax, %ebx      # ebx is the exit code
    sub $1, %ebx        # Exit with zero if ebx is 1
    movl $1, %eax       # put sys_exit code into %eax for the syscall
    int $0x80           # Call kernel sys_exit
