# Tests absolute symbols, as glibc does with locales
# Set an absolute symbol to value 2

.globl abs_symbol
.set abs_symbol, 42

.section .text
.globl _start

_start:
    # Call sys_exit(int status)
    movl $1, %eax               # sys_exit into %eax
    movl $abs_symbol, %ebx;     # Use the address of abs_symbol as the exit code
    int $0x80                   # Call kernel
