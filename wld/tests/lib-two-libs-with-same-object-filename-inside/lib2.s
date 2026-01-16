.globl f2

.section .text
f2:
    mov v2, %eax            # Load v2 into eax and return
    ret

.type	v2, @object
.size   v2, 4
v2:     .long 4
