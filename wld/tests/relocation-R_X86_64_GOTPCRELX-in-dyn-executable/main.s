    .text
    .globl main
    .type main, @function

main:
    pushq %rbp
    movq  %rsp, %rbp

    movl  $42, %edi
    call  *foo@GOTPCREL(%rip)
    cmpl  $43, %eax
    je    .ok
    movl  $1, %edi
    call  exit@PLT

.ok:
    movl  $0, %edi
    call  exit@PLT
