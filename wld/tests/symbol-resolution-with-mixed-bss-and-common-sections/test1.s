# Allocate two symbols in .bss. This C code does this:
# __attribute__((weak))
# int i;
# ...

    .bss

    .align  4
    .weak	i
    .type	i, @object
    .size	i, 4
i:  .zero	4

    .align  4
    .weak	j
    .type	j, @object
    .size	j, 4
j:  .zero	4

# Declare a common symbol, which will also end up in the .bss
.comm k, 4, 4

.text
.globl _start
_start:
    movl $1, %eax       # sys_exit
    movl $1, i(%rip)
    movl $2, j(%rip)
    movl $4, k(%rip)
    mov i(%rip), %ebx
    add j(%rip), %ebx
    add k(%rip), %ebx   # 1 + 2 + 4 = 7
    int $0x80           # Call kernel
