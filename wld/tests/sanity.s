# The simplest possible link test. An object file with no dependencies on libc and no relocations.
# It just exits with a magic code.

.section .text
.globl _start

_start:
    # Call sys_exit(int status)
    movl $1, %eax       # sys_exit into %eax
    movl $42, %ebx;
    int $0x80           # Call kernel
