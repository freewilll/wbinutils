.section .text
.globl f1
f1:
    movq data@GOTPCREL(%rip),  %rax
    movq (%rax), %rax
    ret

.globl f1l
f1l:
    movq local_data1,  %rax
    ret

.section .data
data: .quad 2
local_data1: .quad 10
