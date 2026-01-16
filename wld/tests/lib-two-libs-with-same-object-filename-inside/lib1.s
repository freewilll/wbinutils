.globl f1

.section .text
f1:
    mov v1, %eax        # Load v1 into eax and return
    ret

.type	v1, @object
.size   v1, 4
v1:     .long 5
