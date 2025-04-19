# Define some functions called by crt1.s
# These would normally be linked in with libc

.section .text
.globl __libc_start_main
.globl __libc_csu_fini
.globl __libc_csu_init

__libc_start_main:
    call *main@GOTPCREL(%rip)
    movl %eax, %ebx;
    movl $1, %eax;
    int $0x80

__libc_csu_fini:
    ret

__libc_csu_init:
    ret

.section .data
.globl external_data
external_data:
    .quad 200
