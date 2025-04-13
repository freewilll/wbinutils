# Test the linker setting of the symbols for preinit_array, init_array and fini_array.
# This simulates what libc does on startup/finish

.section .text
.globl _start

preinit_arrays:
    movq    $__preinit_array_start, %rsi
    movq    $__preinit_array_end, %rdx
preloop:
    cmpq    %rsi, %rdx
    je      predone

    movq    (%rsi), %rax     # Load function pointer
    addq    $8, %rsi         # Move to next function
    call    *%rax            # Call the function
    jmp     preloop
predone:
    ret

init_arrays:
    movq    $__init_array_start, %rsi
    movq    $__init_array_end, %rdx
initloop:
    cmpq    %rsi, %rdx
    je      initdone

    movq    (%rsi), %rax     # Load function pointer
    addq    $8, %rsi         # Move to next function
    call    *%rax            # Call the function
    jmp     initloop
initdone:
    ret

# In reality, in libc this loop goes backwards
fini_arrays:
    movq    $__fini_array_start, %rsi
    movq    $__fini_array_end, %rdx
finiloop:
    cmpq    %rsi, %rdx
    je      finidone

    movq    (%rsi), %rax     # Load function pointer
    addq    $8, %rsi         # Move to next function
    call    *%rax            # Call the function
    jmp     finiloop
finidone:
    ret

preinit_array_function1:
    movq $1, data1
    ret

preinit_array_function2:
    movq $2, data2
    ret

init_array_function1:
    movq $3, data3
    ret

init_array_function2:
    movq $4, data4
    ret

fini_array_function1:
    movq $5, data5
    ret

fini_array_function2:
    movq $6, data6
    ret

_start:
    call preinit_arrays
    call init_arrays
    call fini_arrays

    movq data1, %rax
    cmp $1, %rax
    jne error

    movq data2, %rax
    cmp $2, %rax
    jne error

    movq data3, %rax
    cmp $3, %rax
    jne error

    movq data4, %rax
    cmp $4, %rax
    jne error

    movq data5, %rax
    cmp $5, %rax
    jne error

    movq data6, %rax
    cmp $6, %rax
    jne error

    movl $0, %ebx   # exit(0)
    movl $1, %eax
    int $0x80

error:
    movl $1, %ebx   # exit(1)
    movl $1, %eax
    int $0x80

.section .preinit_array
    .quad preinit_array_function1
    .quad preinit_array_function2

.section .init_array
    .quad init_array_function1
    .quad init_array_function2

.section .fini_array
    .quad fini_array_function1
    .quad fini_array_function2

.section .data
    data1: .quad 0
    data2: .quad 0
    data3: .quad 0
    data4: .quad 0
    data5: .quad 0
    data6: .quad 0
