    .text
    .globl main
    .type main, @function

main:
    pushq %rbp
    movq  %rsp, %rbp

    # Indirect via GOT
    movl  $42, %edi
    call  *foo@GOTPCREL(%rip)    # R_X86_64_GOTPCRELX
    cmpl  $43, %eax
    jne   fail

    # Direct PLT call
    movl  $42, %edi
    call  foo                   # R_X86_64_PLT32
    cmpl  $43, %eax
    jne   fail

    movl  $0, %edi
    call  exit@PLT

fail:
    movl  $1, %edi
    call  exit@PLT
