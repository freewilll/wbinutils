.section .text
.globl f2
f2:
    movq global_data@GOTPCREL(%rip),  %rax
    movq (%rax), %rax
    ret

.globl f2l
f2l:
    movq local_data2,  %rax
    ret

local_data2: .quad 20
