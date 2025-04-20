# This tests two things:
# - Optionally linking in a lib that defines a weak symbol, e.g. like the pthread library does
# - Adding a GOT entry for an instruction that can't be relaxed: the cmpq .. @GOTPCREL line

.section .text
.globl _start
.global foo

# Declare optional_function as weak, meaning it will get a symbol value of zero if not linked in
.weak optional_function

_start:
    cmpq $0, optional_function@GOTPCREL(%rip)   # Is optional_function linked in?
    je ok                                       # No, exit with 0

    call optional_function
    movq foo, %rax
    cmpq $200, %rax
    jne exit1
    je exit2

ok:
    movl $0, %ebx   # exit(0)
    movl $1, %eax
    int $0x80

# Error
exit1:
    movl $1, %ebx;  # exit(1)
    movl $1, %eax
    int $0x80

# The optional function was called & verified, exit with 2
exit2:
    movl $2, %ebx;  # exit(2)
    movl $1, %eax
    int $0x80

.section .data
foo: .quad 0
