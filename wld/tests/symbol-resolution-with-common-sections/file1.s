.comm i, 4, 4 # Two commons
.comm k, 4, 4 # Defined in file 2

.section .data
.global j
.type	j, @object
.size j, 4
.align 4
j: .long 1

.text
.globl _start
_start:
    movl $1, %eax       # sys_exit
    movl i, %ebx;       # 0
    movl j, %ecx;       # 1
    imul $2, %ecx
    add %ecx, %ebx
    movl k, %edx;       # 2
    imul $4, %ecx
    add %ecx, %ebx      # 10
    int $0x80           # Call kernel

