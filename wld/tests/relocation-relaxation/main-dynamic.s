# Test relocation relaxation based on what's in crt1.o and crti.o
# These would normally be linked in with libc

.section .text
.globl main

exit_with_not_ok:
    movl $1, %eax;
    ret

# Test linker opcode rewriting.
# In the static case, the instructions are changed by the linker to not use a GOT.
# In the dynamic case, instrutions are relaxed for GOTPCRELX relocations, or
# GOT entries + relocations are added for GOTPCREL.
main:
    # Test R_X86_64_GOTPCREL* relocations
    movq reg00@GOTPCREL(%rip), %rax
    movq (%rax), %rax ; cmp $100, %rax
    jne exit_with_not_ok

    movq reg00@GOTPCREL(%rip), %rcx
    movq (%rcx), %rax ; cmp $100, %rax
    jne exit_with_not_ok

    # Check a GOTPCREL for an undefined weak symbol works as it should
    .weak weak_fn        # undefined weak symbol
    movq weak_fn@GOTPCREL(%rip), %rax
    test %rax, %rax
    jne exit_with_not_ok

     # Check a GOTPCREL for an undefined weak symbol works as it should
    .weak weak_fn        # undefined weak symbol
    movq weak_fn@GOTPCREL(%rip), %rax
    test %rax, %rax
    jne exit_with_not_ok

    movl $0, %eax;
    ret

.section .data
reg00: .quad 100
reg01: .quad 101
reg02: .quad 102
reg03: .quad 103
reg04: .quad 104
reg05: .quad 105
reg06: .quad 106
reg07: .quad 107
reg08: .quad 108
reg09: .quad 109
reg10: .quad 110
reg11: .quad 111
reg12: .quad 112
reg13: .quad 113
reg14: .quad 114
reg15: .quad 115
