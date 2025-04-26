.section .text
.globl _start

_start:
   .weak f
    mov $f, %rax
    cmp $0, %rax
    jne f_defined
    mov $0, %ebx
    jmp exit

f_defined:
    call f
    mov %eax, %ebx      # ebx is the exit code

exit:
    movl $1, %eax       # put sys_exit code into %eax for the syscall
    int $0x80           # Call kernel sys_exit
