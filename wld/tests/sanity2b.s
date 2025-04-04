.globl j
.size j, 1
.globl k
.globl f
.globl msg
.globl msg_len

.section .data
j: .byte 18     # Read by the other file
k: .byte 22     # Read by the other file

msg: .asciz "Hello World!\n"  # Read by the other file
msg_len: .long 13 # Read by the other file

# Called by the other file
.section .text
f:
    mov i, %eax # i is present in the other file
    ret
