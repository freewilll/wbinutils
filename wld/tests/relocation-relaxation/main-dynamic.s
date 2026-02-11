exit_with_not_ok:
    movl $1, %eax;
    ret

.section .text
.globl main
main:
    # Check a GOTPCREL for an undefined weak symbol works as it should
    .weak weak_fn        # undefined weak symbol
    movq weak_fn@GOTPCREL(%rip), %rax
    test %rax, %rax
    jne exit_with_not_ok

    movl $0, %eax;
    ret
