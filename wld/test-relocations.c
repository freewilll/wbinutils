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

void run_relocation(void *output_data, uint64_t output_offset, int type, int addend, uint32_t output_virtual_address, uint64_t value) {
    ElfRelocation relocation = {.r_info = type, .r_addend = addend};

    int result = apply_relocation(&relocation, output_virtual_address, value, output_data, output_offset);
    if (result) {
        printf("Relocation failed\n");
        exit(1);
    }
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
}

void test_R_X86_64_32s(int type) {
    uint32_t output_data[4]; memset(output_data, -1, 16);
    run_relocation(output_data, 8, type, 0x10, 0x400000, 0x401000);
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

int main() {
    test_R_X86_64_64();
    test_R_X86_64_PC32();
    test_R_X86_64_32s(R_X86_64_32);
    test_R_X86_64_32s(R_X86_64_32S);
    test_R_X86_64_GOTPCRELX();
    test_R_X86_64_REX_GOTPCRELX();
}