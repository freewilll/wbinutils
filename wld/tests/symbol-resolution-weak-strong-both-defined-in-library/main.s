.section .text
.globl _start

_start:
    call g              # Include test2.o file by calling g
    call f              # the strong symbol f() in test2.o takes precedence over the weak symbol in test1.o
    mov %eax, %ebx      # ebx is the exit code
    sub $2, %ebx        # Return zero if ebx is 2
    movl $1, %eax       # put sys_exit code into %eax for the syscall
    int $0x80           # Call kernel sys_exit
