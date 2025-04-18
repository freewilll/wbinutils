# This is an end to end test of TLS which prepares the main thread for execution
# by allocating memory, setting up %fs (the base address of the TLS pointer),
# copying a TLS template to the memory and then reading from it.
# This uses the local exec model, where the TLS template is before the TLS pointer.
# foo is at offset -16 in .tdata
# bar is at offset -8 in .tbss

.globl foo
.globl bar
.globl _start

# Put an integer in .tdata. The offset will be -8 from the TLS base address
.section .tdata, "awT", @progbits
foo:
    .align 8
    .quad 42                      # Thread-local variable

# Put an integer in .tbss. The offset will be -4 from the TLS base address
.section .tbss, "awT", @nobits
bar:
    .align 8
    .skip 8     # Reserve 4 bytes of TLS space (zero-initialized)

.section .text
_start:
    # Allocate a page of memory to copy the TLS template to

    # mmap(NULL, 4096, PROT_READ|WRITE, MAP_PRIVATE|ANONYMOUS, -1, 0)
    mov $9, %rax                    # Syscall: mmap
    mov $0, %rdi                    # Address = NULL
    mov $4096, %rsi                 # Size
    mov $3, %rdx                    # PROT_READ | PROT_WRITE
    mov $0x22, %r10                 # MAP_PRIVATE | MAP_ANONYMOUS
    mov $-1, %r8                    # fd
    mov $0, %r9                     # offset
    syscall
    mov %rax, %r12                  # Put the TLS base into %r12

    # move %fs to end of TLS block
    add $4096, %r12                 # %fs = base + size

    # copy initial TLS data (4 bytes)
    lea foo(%rip), %rsi             # This is an unusual thing to do. Normally a TLS var would not be read from the TLS template.
                                    # However, in this case it's ok since we're pretending to be libc and setting up TLs.

    # Check that %rsi points at foo
    movq (%rsi), %rax
    cmp $42, %rax
    jne not_ok

    mov %r12, %rdi
    sub $16, %rdi                   # destination: (%fs - 8)
    mov $8, %ecx                    # Copy r bytes
    rep movsb

    # Clear .tbss (bar)
    mov %r12, %rdi
    sub $8, %rdi                   # bar goes at offset -8
    mov $8, %rcx                   # size of .tbss section (4 bytes)
    mov $0, %eax

clear_tbss_loop:
    movb %al, (%rdi)
    inc %rdi
    dec %rcx
    jnz clear_tbss_loop

    # set %fs to %r12 using the arch_prctl syscall
    mov $158, %eax                  # syscall: arch_prctl
    mov $0x1002, %edi               # ARCH_SET_FS
    mov %r12, %rsi                  # TLS base
    syscall
    cmp $0, %rax                    # Check return code of the syscall
    jne not_ok

    # R_X86_64_TPOFF32 relocations
    movq %fs:foo@TPOFF, %rax ; cmp $42, %rax ; jne not_ok
    movq %fs:foo@TPOFF, %rcx ; cmp $42, %rcx ; jne not_ok
    movq %fs:foo@TPOFF, %rbp ; cmp $42, %rbp ; jne not_ok
    movq %fs:foo@TPOFF, %r9  ; cmp $42, %r9  ; jne not_ok

    movq %fs:bar@TPOFF, %rax ; cmp $0, %rax  ; jne not_ok

ok:
    mov $0, %edi                    # Return zero if %edi is 42, the expected value
    mov $60, %eax                   # exit(%edi)
    syscall

not_ok:
    mov $1, %edi                    # exit(1)
    mov $60, %eax
    syscall
