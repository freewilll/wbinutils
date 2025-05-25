# This is and end to end test which replicates glibc's startup code related to IFUNCs.
# The linker is expected to set __rela_iplt_start and __rela_iplt_end. These contain
# a list of relocations. Each relocation has a pointer to a resolver function, which
# return a pointer to the actual function. This pointer ends up getting written to the
# GOT, which works together with the PLT to make it callable.

.globl _start

_start:
    call    resolve_ifuncs
    call    foo_global@PLT
    cmp     $42, %rax

    call    resolve_ifuncs
    call    foo_global
    cmp     $42, %rax

    call    foo_local@PLT
    cmp     $43, %rax
    jne     not_ok

    call    foo_local
    cmp     $43, %rax
    jne     not_ok

    # Check the GOT table entry for foo_global points at foo_global_func
    lea     foo_global@GOTPCREL(%rip), %rax
    mov     (%rax), %rax
    cmp     $foo_global_func, %rax
    jne     not_ok

    # # Check the GOT table entry for foo_local points at foo_local_func
    lea     foo_local@GOTPCREL(%rip), %rax
    mov     (%rax), %rax
    cmp     $foo_local_func, %rax
    jne     not_ok

ok:
    mov     $0, %edi    # exit(0)
    mov     $60, %eax
    syscall

not_ok:
    mov     $1, %edi    # exit(1)
    mov     $60, %eax
    syscall

# Loop over all relocations and handle those of type R_X86_64_IRELATIVE
resolve_ifuncs:
    mov     $__rela_iplt_start, %rsi
    mov     $__rela_iplt_end, %rdi

resolve_ifuncs_loop:
    cmp     %rsi, %rdi
    je      resolve_ifuncs_done

    mov     (%rsi), %r8         # offset
    mov     8(%rsi), %ebx       # info
    mov     16(%rsi), %rcx      # addend = address of resolver

    cmp     $37, %ebx           # R_X86_64_IRELATIVE
    jne     resolve_ifuncs_next_loop

    call    *%rcx               # Call resolver
    mov     %rax, (%r8)         # Store resolved address at offset

resolve_ifuncs_next_loop:
    add     $24, %rsi
    jmp     resolve_ifuncs_loop

resolve_ifuncs_done:
    ret

# The implementation of the resolved foo_global
foo_global_func:
    mov     $42, %rax
    ret

# Ifunc for foo_global. Returns a pointer to foo_global_func.
foo_global_ifunc:
    mov     $foo_global_func, %rax
    ret

# Define foo_global as an IFUNC, using foo_global_ifunc for its resolver
.globl foo_global # Global, to test the handling of global ifuncs
.type   foo_global, @gnu_indirect_function
.set    foo_global, foo_global_ifunc

# The implementation of the resolved foo_local
foo_local_func:
    mov     $43, %rax
    ret

# Ifunc for foo_local. Returns a pointer to foo_local_func.
foo_local_ifunc:
    mov     $foo_local_func, %rax
    ret

# Define foo_local as an IFUNC, using foo_local_ifunc for its resolver
# foo_local is a local symbol, to test the handling of local ifuncs
.type   foo_local, @gnu_indirect_function
.set    foo_local, foo_local_ifunc
