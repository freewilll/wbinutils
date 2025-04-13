.section .text
.globl f1
f1:
    movq data@GOTPCREL(%rip),  %rax
    movq (%rax), %rax
    ret

.section .data
data: .quad 2
