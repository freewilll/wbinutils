.section .text
.globl f2
f2:
    movq global_data@GOTPCREL(%rip),  %rax
    movq (%rax), %rax
    ret
