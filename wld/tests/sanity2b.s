.globl j
.globl k
.globl f

.section .data
j: .byte 18     # Read by the other file
k: .byte 22     # Read by the other file

# Called by the other file
.section .text
f:
    mov i, %eax # i is present in the other file
    ret
