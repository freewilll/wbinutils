#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "elf.h"

#include "wld/relocations.h"

#define END -1

static void assert_uint32(uint32_t expected, uint32_t actual, const char *message) {
    if (expected != actual)
        panic("%s: expected %#x, got %#x", message, expected, actual);
}

static void assert_uint64(uint64_t expected, uint64_t actual, const char *message) {
    if (expected != actual)
        panic("%s: expected %#lx, got %#lx", message, expected, actual);
}

// Print hex bytes
static int hexdump(char *data, int size) {
    for (int i = 0; i < size; i++) {
        if (i != 0) printf(", ");
        printf("0x%02x", (unsigned char) data[i]);
    }
    printf("\n");
}

void vassert_data(char *data, va_list ap) {
    int pos = 0;

    while (1) {
        unsigned int expected = va_arg(ap, unsigned int);

        if (expected == END) return;

        if ((expected & 0xff) != (data[pos] & 0xff)) {
            hexdump(data, pos + 1);
            panic("Mismatch at position %d: expected %#02x, got %#02x", pos, (uint8_t) expected & 0xff, data[pos] & 0xff);
        }

        pos++;
    }
}

void assert_data(char *data, ...) {
    va_list ap;
    va_start(ap, data);
    vassert_data(data, ap);
    va_end(ap);
}

void run_full_relocation(int output_type, void *output_data, uint64_t output_offset, uint64_t tls_template_address, int tls_template_size, int type, int addend, uint32_t output_virtual_address, uint64_t value, int is_tls_value, uint64_t value_got_offset, uint64_t value_iplt_offset, uint64_t value_got_iplt_offset, uint64_t value_plt_offset, int link_dynamically) {
    OutputElfFile *output_elf_file = new_output_elf_file("", output_type);
    output_elf_file->tls_template_address = tls_template_address;
    output_elf_file->tls_template_size = tls_template_size;

    ElfRelocation relocation = {.r_offset = output_offset, .r_info = type, .r_addend = addend};

    output_elf_file->got_virt_address = 0x500000;
    output_elf_file->iplt_virt_address = 0x600000;
    output_elf_file->got_iplt_virt_address = 0x700000;
    output_elf_file->plt_offset = 0x8000;
    uint64_t rw_section_address = output_virtual_address - output_offset;
    uint64_t rw_section_offset = rw_section_address - 0x400000;

    int result = scan_relocation(output_data, 0, &relocation);
    if (result == SCAN_RELOCATION_ERROR) panic("Relocation scan failed with %d", result);

    result = apply_relocation(output_elf_file, output_data, rw_section_offset, rw_section_address, output_offset, link_dynamically, &relocation,
        is_tls_value,
        value,
        value_plt_offset,
        value_iplt_offset,
        value_got_offset,
        value_got_iplt_offset
);
    if (result) panic("Relocation apply failed");
}

// Runs a relocation with TLS args
void run_tls_relocation(void *output_data, uint64_t output_offset, uint64_t tls_template_address, int tls_template_size, int type, int addend, uint32_t output_virtual_address, uint64_t value, int is_tls_value) {
    run_full_relocation(ET_EXEC, output_data, output_offset, tls_template_address, tls_template_size, type, addend, output_virtual_address, value, is_tls_value, -1, -1, -1, -1, 0);
}

// Runs a relocation with a GOT value
void run_got_relocation(void *output_data, uint64_t output_offset, int type, int addend, uint32_t output_virtual_address, uint64_t value, uint64_t value_got_offset) {
    run_full_relocation(ET_EXEC, output_data, output_offset, 0, 0, type, addend, output_virtual_address, value, 0, value_got_offset, -1, -1, -1, 0);
}

// Runs a relocation with a .got.iplt value
void run_got_iplt_relocation(void *output_data, uint64_t output_offset, int type, int addend, uint32_t output_virtual_address, uint64_t value, uint64_t value_got_iplt_offset) {
    run_full_relocation(ET_EXEC, output_data, output_offset, 0, 0, type, addend, output_virtual_address, value, 0, -1, -1, value_got_iplt_offset, -1, 0);
}

// Runs a relocation with an .iplt value
void run_iplt_relocation(void *output_data, uint64_t output_offset, int type, int addend, uint32_t output_virtual_address, uint64_t value, uint64_t value_iplt_offset) {
    run_full_relocation(ET_EXEC, output_data, output_offset, 0, 0, type, addend, output_virtual_address, value, 0, -4, value_iplt_offset, -1, -1, 0);
}

// Runs a relocation with an .plt value
void run_plt_relocation(void *output_data, uint64_t output_offset, int type, int addend, uint32_t output_virtual_address, uint64_t value, uint64_t value_plt_offset, int link_dynamically) {
    run_full_relocation(ET_DYN, output_data, output_offset, 0, 0, type, addend, output_virtual_address, value, 0, -4, -1, -1, value_plt_offset, link_dynamically);
}

// Runs a regular relocation
void run_relocation(void *output_data, uint64_t output_offset, int type, int addend, uint32_t output_virtual_address, uint64_t value) {
    run_tls_relocation(output_data, output_offset, 0, 0, type, addend, output_virtual_address, value, -1);
}

void test_R_X86_64_64(void) {
    uint64_t output_data[4]; memset(output_data, -1, 32);
    run_relocation(output_data, 8, R_X86_64_64, 0x10, 0x400000, 0x401000);
    assert_uint64(-1, output_data[0], "R_X86_64_64");
    assert_uint64(0x401010, output_data[1], "R_X86_64_64");
    assert_uint64(-1, output_data[2], "R_X86_64_64");
}

void test_R_X86_64_PC32(void) {
    uint32_t output_data[4]; memset(output_data, -1, 16);
    run_relocation(output_data, 8, R_X86_64_PC32, 0x10, 0x400000, 0x401000);
    assert_uint32(-1, (output_data)[1], "R_X86_64_PC32");
    assert_uint32(0x1010, (output_data)[2], "R_X86_64_PC32");
    assert_uint32(-1, (output_data)[3], "R_X86_64_PC32");

    // Unusual case of accessing a TLS template variable directly.
    // This is mostly to make an unusual TLS test case work.
    // The program counter is 0x401000
    // The address of the TLS template is 0x402000
    // So the relative address is 0x402000 - 0x401000 = 0x1000
    uint8_t output_data2[16]; memset(output_data, -1, 16);
    run_tls_relocation(output_data2, 0, 0x402000, 8, R_X86_64_PC32, 0, 0x401000, 0, 1);
    assert_data(output_data2, 0x00, 0x10, 0x00, 0x00, END);

    // Same as above, but with an addend of 1
    uint8_t output_data3[16]; memset(output_data, -1, 16);
    run_tls_relocation(output_data3, 0, 0x402000, 8, R_X86_64_PC32, 1, 0x401000, 0, 1);
    assert_data(output_data3, 0x01, 0x10, 0x00, 0x00, END);
}

void test_R_X86_64_32s(int type) {
    uint32_t output_data[4]; memset(output_data, -1, 16);
    run_relocation(output_data, 8,  type, 0x10, 0x400000, 0x401000);
    assert_uint32(-1, (output_data)[1], "R_X86_64_32*");
    assert_uint32(0x401010, (output_data)[2], "R_X86_64_32*");
    assert_uint32(-1, (output_data)[3], "R_X86_64_32*");
}

int test_R_X86_64_GOTPCRELX(void) {
    uint8_t output_data[] = {0x8b, 0x0d, 0x00, 0x00, 0x00, 0x00};                   // mov 0x0(%rip), %ecx
    run_relocation(output_data, 2, R_X86_64_GOTPCRELX, 0x10, 0x400000, 0x401000);
    assert_data(output_data, 0xc7, 0xc1, 0x00, 0x10, 0x40, 0x00, END);              // mov $0x401000, %ecx

    uint8_t output_data2[] = {0xff, 0x15, 0x00, 0x00, 0x00, 0x00};                  // callq *0x0(%rip)
    run_relocation(output_data2, 2, R_X86_64_GOTPCRELX, 0x10, 0x400000, 0x401000);
    assert_data(output_data2, 0x67, 0xe8, 0x10, 0x10, 0x00, 0x00, END);             // addr32 callq 401000
}

int test_R_X86_64_REX_GOTPCRELX() {
    // mov
    uint8_t output_data[] = {0x48, 0x8b, 0x0d, 0x00, 0x00, 0x00, 0x00};             // mov 0x0(%rip), %rcx
    run_relocation(output_data, 3, R_X86_64_REX_GOTPCRELX, 0x10, 0x400000, 0x401000);
    assert_data(output_data, 0x48, 0xc7, 0xc1, 0x00, 0x10, 0x40, 0x00, END);        // mov $0x401000, %rcx

    uint8_t output_data2[] = {0x4c, 0x8b, 0x0d, 0x00, 0x00, 0x00, 0x00};            // mov 0x0(%rip), %r9
    run_relocation(output_data2, 3, R_X86_64_REX_GOTPCRELX, 0x10, 0x400000, 0x401000);
    assert_data(output_data2, 0x49, 0xc7, 0xc1, 0x00, 0x10, 0x40, 0x00, END);       // mov $0x401000, %r9

    // cmp
    uint8_t output_data3[] = {0x4c, 0x8b, 0x0d, 0x00, 0x00, 0x00, 0x00};            // cmp 0x0(%rip), %rcx
    run_relocation(output_data3, 3, R_X86_64_REX_GOTPCRELX, 0x10, 0x400000, 0x401000);
    assert_data(output_data3, 0x49, 0xc7, 0xc1, 0x00, 0x10, 0x40, 0x00, END);       // cmp $0x401000, %rcx

    uint8_t output_data4[] = {0x4c, 0x3b, 0x0d, 0x00, 0x00, 0x00, 0x00};            // cmp 0x0(%rip), %r9
    run_relocation(output_data4, 3, R_X86_64_REX_GOTPCRELX, 0x10, 0x400000, 0x401000);
    assert_data(output_data4, 0x49, 0x81, 0xf9, 0x00, 0x10, 0x40, 0x00, END);       // cmp $0x401000, %r9

    // sub
    uint8_t output_data5[] = {0x48, 0x2b, 0x0d, 0x00, 0x00, 0x00, 0x00};            // sub 0x0(%rip), %rcx
    run_relocation(output_data5, 3, R_X86_64_REX_GOTPCRELX, 0x10, 0x400000, 0x401000);
    assert_data(output_data5, 0x48, 0x81, 0xe9, 0x00, 0x10, 0x40, 0x00, END);       // sub $0x401000, %rcx

    uint8_t output_data6[] = {0x4c, 0x2b, 0x0d, 0x00, 0x00, 0x00, 0x00};            // sub 0x0(%rip), %r9
    run_relocation(output_data6, 3, R_X86_64_REX_GOTPCRELX, 0x10, 0x400000, 0x401000);
    assert_data(output_data6, 0x49, 0x81, 0xe9, 0x00, 0x10, 0x40, 0x00, END);       // sub $0x401000, %r9
}

void test_R_X86_64_TPOFF32() {
    // With a TLS template size of 8, and symbol value of 0, the relative offset to the end of the TLS template is -8.
    uint8_t output_data1[] = {0x00, 0x00, 0x00, 0x00};
    run_tls_relocation(output_data1, 0, 0, 8, R_X86_64_TPOFF32, 0, 0x400000, 0, 1);
    assert_data(output_data1, 0xf8, 0xff, 0xff, 0xff, END);
}

void test_R_X86_64_GOTPCREL_with_GOT_entry() {
    // This instruction cannot be relaxed, so a GOT entry has to be added for the symbol
    uint8_t output_data[] = {0x48, 0x83, 0x3d, 0xf8, 0x00, 0x00, 0x00, 0x00};      // cmpq $0, foo@GOTPCREL(%rip)
    run_got_relocation(output_data, 4, R_X86_64_GOTPCREL, 0, 0x400000, 0x401000, 0x20);
    assert_data(output_data, 0x48, 0x83, 0x3d, 0xf8, 0x20, 0x00, 0x10, 0x00, END); // 0x500000 - 0x400000 + 0x20 = 0x00100020
}

void test_R_X86_64_GOTPCREL_with_GOT_iplt_entry() {
    // This instruction references something in the .got.plt.
    uint8_t output_data[] = {0x48, 0x83, 0x3d, 0xf8, 0x00, 0x00, 0x00, 0x00};      // cmpq $0, foo@GOTPCREL(%rip)
    run_got_iplt_relocation(output_data, 4, R_X86_64_GOTPCREL, 0, 0x400000, 0x401000, 0x20);
    assert_data(output_data, 0x48, 0x83, 0x3d, 0xf8, 0x20, 0x00, 0x30, 0x00, END); // 0x700000 - 0x400000 + 0x20 = 0x00100020
}

void test_R_X86_64_PLT32_in_static(void) {
    // For entries not in the PLT, R_X86_64_PLT32 behaves like a R_X86_64_PC32
    uint32_t output_data[4]; memset(output_data, -1, 16);
    run_relocation(output_data, 8, R_X86_64_PLT32, 0x10, 0x400000, 0x401000);
    assert_uint32(-1, (output_data)[1], "R_X86_64_PLT32");
    assert_uint32(0x1010, (output_data)[2], "R_X86_64_PLT32");
    assert_uint32(-1, (output_data)[3], "R_X86_64_PLT32");

    // The symbol value is in the .iplt section
    uint8_t output_data2[] = {0xe8, 0x00, 0x00, 0x00, 0x00};      // callq foo@PLT and callq foo
    run_iplt_relocation(output_data2, 1, R_X86_64_PLT32, 0, 0x400000, 0x401000, 0x20);
    assert_data(output_data2, 0xe8, 0x20, 0x00, 0x20, 0x00, END); // 0x600000 - 0x400000 + 0x20 = 0x00200020
}

// ET_DYN outputs are either executables or shared libraries.
// Symbols in the PLT gets statically linked in executables and dynamically linked in shared libraries
void test_R_X86_64_PLT32_in_shared(void) {
    // When linking dynamically, the symbol value is in the .plt section
    uint8_t output_data1[] = {0xe8, 0x00, 0x00, 0x00, 0x00};      // callq foo@PLT and callq foo
    run_plt_relocation(output_data1, 1, R_X86_64_PLT32, 0, 0x0, 0x10, 0x20, 1);
    assert_data(output_data1, 0xe8, 0x20, 0x80, 0x00, 0x00, END); // S(0x8020) = plt_offset(0x8000) + value_plt_offset(0x20) + A(0) - P(0);

    // When linking statically, R_X86_64_PLT32 is converted to R_X86_64_PC32
    uint8_t output_data2[] = {0xe8, 0x00, 0x00, 0x00, 0x00};      // callq foo@PLT and callq foo
    run_plt_relocation(output_data2, 1, R_X86_64_PLT32, 0, 0x0, 0x10, 0x20, 0);
    assert_data(output_data2, 0xe8, 0x10, 0x00, 0x00, 0x00, END); // The symbol value of 0x10 is used
}

int main() {
    test_R_X86_64_64();
    test_R_X86_64_PC32();
    test_R_X86_64_32s(R_X86_64_32);
    test_R_X86_64_32s(R_X86_64_32S);
    test_R_X86_64_GOTPCRELX();
    test_R_X86_64_REX_GOTPCRELX();
    test_R_X86_64_TPOFF32();
    test_R_X86_64_GOTPCREL_with_GOT_entry();
    test_R_X86_64_GOTPCREL_with_GOT_iplt_entry();
    test_R_X86_64_PLT32_in_static();
    test_R_X86_64_PLT32_in_shared();
}
