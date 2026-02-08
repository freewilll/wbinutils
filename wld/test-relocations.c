#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "elf.h"
#include "input-elf.h"

#include "wld/relocations.h"
#include "wld/symbols.h"

#define END -1

typedef enum output_type {
    OT_STATIC_EXEC,  // Statically linked executable
    OT_DYN_EXEC,     // Dynamically linked executable
    OT_SHLIB         // Shared library
} OutputType;

struct state {
    OutputElfFile *output_elf_file;
    InputElfFile *input_elf_file;
    InputSection  *input_section;
    OutputSection *output_section;
    void *output_data;
    ElfRelocation *relocation;
    Symbol *symbol;
    uint32_t output_virtual_address;
} state;

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

static void reset(OutputType ot, void *output_data, uint64_t reloc_type, uint64_t reloc_offset, uint64_t reloc_addend) {
    int elf_output_type;
    int is_executable;

    switch(ot) {
        case OT_STATIC_EXEC:
            elf_output_type = ET_EXEC;
            is_executable = 1;
            break;
        case OT_DYN_EXEC:
            elf_output_type = ET_DYN;
            is_executable = 1;
            break;
        case OT_SHLIB:
            elf_output_type = ET_DYN;
            is_executable = 0;
            break;
        default:
            printf("Bad OT value %d\n", ot);
            exit(1);
    }

    // Make the output elf file
    OutputElfFile *output_elf_file = new_output_elf_file("", elf_output_type);
    output_elf_file->is_executable = is_executable;
    output_elf_file->got_virt_address = 0x500000;
    output_elf_file->iplt_virt_address = 0x600000;
    output_elf_file->got_iplt_virt_address = 0x700000;
    output_elf_file->plt_offset = 0x8000;

    // Make the relocation
    ElfRelocation *relocation = calloc(1, sizeof(ElfRelocation));
    relocation->r_offset = reloc_offset;
    relocation->r_info = reloc_type;
    relocation->r_addend = reloc_addend;

    // Make the input section
    InputSection  *input_section = calloc(1, sizeof(InputSection));
    input_section->data = output_data;

    OutputSection *output_section = calloc(1, sizeof(OutputSection));
    input_section->output_section = output_section;
    output_section->data = output_data;
    output_section->address = 0x400000;

    // Make the input ELF file
    InputElfFile *input_elf_file = calloc(1, sizeof(InputElfFile));
    input_elf_file->type = ET_REL;
    input_elf_file->filename = "test-file";

    // Add one entry to the symbol table at index 0. All symbols in this test are at index zero
    input_elf_file->symbol_table = calloc(1, sizeof(ElfSymbol));
    char *test_symbol_name = "test-symbol";
    input_elf_file->symbol_table_strings = test_symbol_name;

    // Add symbols tables
    local_symbol_tables = new_strmap();
    SymbolTable *local_symbol_table = new_symbol_table();
    strmap_put(local_symbol_tables, input_elf_file->filename, local_symbol_table);
    global_symbol_table = new_symbol_table();

    // Add a symbol
    int other = 0;
    int size = 8;
    int version_index = 0;
    int is_default_version = 0;
    Symbol *symbol = new_symbol(strdup(test_symbol_name), STT_OBJECT, STB_GLOBAL, other, size, SRC_INTERNAL);
    symbol->src_elf_file = input_elf_file;
    SymbolNV *snv = new_symbolnv(strdup(test_symbol_name), version_index, is_default_version);
    map_ordered_put(global_symbol_table->defined_symbols, snv, symbol);

    state.output_elf_file = output_elf_file;
    state.input_elf_file = input_elf_file;
    state.input_section = input_section;
    state.output_section = output_section;
    state.output_data = output_data;
    state.relocation = relocation;
    state.symbol = symbol;
    state.output_virtual_address = output_section->address;
}

static void set_output_section_address(uint64_t address) {
    state.output_section->address = address;
}

static void set_output_virtual_address(uint32_t output_virtual_address) {
    state.output_virtual_address = output_virtual_address;
}

static void set_value(uint64_t value) {
    state.symbol->dst_value = value;
}

static void set_plt_offset(uint64_t plt_offset) {
    state.symbol->extra = SE_IN_GOT_PLT;
    state.symbol->plt_offset = plt_offset;
}

static void set_iplt_offset(uint64_t iplt_offset) {
    state.symbol->extra = SE_IN_GOT_IPLT;
    state.symbol->iplt_offset = iplt_offset;
}

static void set_got_iplt_offset(uint64_t got_iplt_offset) {
    state.symbol->extra = SE_IN_GOT_IPLT;
    state.symbol->got_iplt_offset = got_iplt_offset;
}

static void set_got_offset(uint64_t got_offset) {
    state.symbol->extra = SE_IN_GOT;
    state.symbol->got_offset = got_offset;
}

static void set_tls_template_address(uint64_t tls_template_address) {
    state.output_elf_file->tls_template_address = tls_template_address;
}

static void set_tls_template_tls_offset(uint64_t tls_template_tls_offset) {
    state.output_elf_file->tls_template_tls_offset = tls_template_tls_offset;
}

static void run() {
    // In normal execution, SE_IN_GOT_IPLT is set in between scan and apply. This needs simulating here by resetting it in between.
    int in_got_iplt = state.symbol->extra == SE_IN_GOT_IPLT;
    scan_relocation(state.output_elf_file, state.input_elf_file, state.input_section, state.relocation);
    if (in_got_iplt) state.symbol->extra = SE_IN_GOT_IPLT;
    apply_relocation(state.output_elf_file, state.input_elf_file, state.input_section, state.relocation);
}

void test_R_X86_64_64(void) {
    uint64_t output_data[4];
    memset(output_data, -1, 32);

    // The relocation must be applied to a statically linked executable
    reset(OT_STATIC_EXEC, output_data, R_X86_64_64, 8, 0x10);
    set_value(0x401000);
    run();
    assert_uint64(-1, output_data[0], "R_X86_64_64");
    assert_uint64(0x401010, output_data[1], "R_X86_64_64");
    assert_uint64(-1, output_data[2], "R_X86_64_64");

    // The relocation must be applied to a dynamically linked executable
    memset(output_data, -1, 32);
    reset(OT_DYN_EXEC, output_data, R_X86_64_64, 8, 0x10);
    set_value(0x401000);
    run();
    assert_uint64(-1, output_data[0], "R_X86_64_64");
    assert_uint64(0x401010, output_data[1], "R_X86_64_64");
    assert_uint64(-1, output_data[2], "R_X86_64_64");

    // The relocation must be applied to a dynamically linked shared library
    memset(output_data, -1, 32);
    reset(OT_SHLIB, output_data, R_X86_64_64, 8, 0x10);
    set_value(0x401000);
    run();
    assert_uint64(-1, output_data[0], "R_X86_64_64");
    assert_uint64(0x401010, output_data[1], "R_X86_64_64");
    assert_uint64(-1, output_data[2], "R_X86_64_64");
}

void test_R_X86_64_PC32(void) {
    // S + A - P = 0x401000 + 0x10 - 0x400008 = 0x1008
    uint32_t output_data[4]; memset(output_data, -1, 16);
    reset(OT_STATIC_EXEC, output_data, R_X86_64_PC32, 8, 0x10);
    set_value(0x401000);
    run();
    assert_uint32(-1, (output_data)[1], "R_X86_64_PC32");
    assert_uint32(0x1008, (output_data)[2], "R_X86_64_PC32");
    assert_uint32(-1, (output_data)[3], "R_X86_64_PC32");

    // Unusual case of accessing a TLS template variable directly.
    // This is mostly to make an unusual TLS test case work.
    // The program counter is 0x401000
    // The address of the TLS template is 0x402000
    // So the relative address is 0x402000 - 0x401000 = 0x1000
    uint8_t output_data2[16]; memset(output_data, -1, 16);
    reset(OT_STATIC_EXEC, output_data2, R_X86_64_PC32, 0, 0);
    set_value(0x401000);
    set_tls_template_address(0x402000);
    set_tls_template_tls_offset(8);
    set_plt_offset(8);
    run();
    assert_data(output_data2, 0x00, 0x10, 0x00, 0x00, END);

    // Same as above, but with an addend of 1
    uint8_t output_data3[16]; memset(output_data, -1, 16);
    reset(OT_STATIC_EXEC, output_data3, R_X86_64_PC32, 0, 1);
    set_value(0x401000);
    set_tls_template_address(0x402000);
    set_tls_template_tls_offset(8);
    set_plt_offset(8);
    run();
    assert_data(output_data3, 0x01, 0x10, 0x00, 0x00, END);
}

void test_R_X86_64_32s(int type) {
    uint32_t output_data[4]; memset(output_data, -1, 16);
    reset(OT_STATIC_EXEC, output_data, type, 8, 0x10);
    set_value(0x401000);
    run();
    assert_uint32(-1, (output_data)[1], "R_X86_64_32*");
    assert_uint32(0x401010, (output_data)[2], "R_X86_64_32*");
    assert_uint32(-1, (output_data)[3], "R_X86_64_32*");
}

int test_R_X86_64_GOTPCRELX(void) {
    uint8_t output_data[] = {0x8b, 0x0d, 0x00, 0x00, 0x00, 0x00};                   // mov 0x0(%rip), %ecx
    reset(OT_STATIC_EXEC, output_data, R_X86_64_GOTPCRELX, 2, 0x10);
    set_value(0x401000);
    run();
    assert_data(output_data, 0xc7, 0xc1, 0x00, 0x10, 0x40, 0x00, END);              // mov $0x401000, %ecx

    // S + A - P =  0x401000 + 0x10 - 0x401002 = 0x100e
    uint8_t output_data2[] = {0xff, 0x15, 0x00, 0x00, 0x00, 0x00};                  // callq *0x0(%rip)
    reset(OT_STATIC_EXEC, output_data2, R_X86_64_GOTPCRELX, 2, 0x10);
    set_value(0x401000);
    run();
    assert_data(output_data2, 0x67, 0xe8, 0x0e, 0x10, 0x00, 0x00, END);             // addr32 callq *0x100e(%rip)
}

int test_R_X86_64_GOTPCRELX_in_shared_object(void) {
    uint8_t output_data[] = {0xff, 0x15, 0x00, 0x00, 0x00, 0x00};                   // call *foo(%rip)
    reset(OT_DYN_EXEC, output_data, R_X86_64_GOTPCRELX, 2, 0);
    state.symbol->src_elf_file->type = ET_DYN;
    set_got_offset(0x20);
    run();
    // got_address + got_offset + A - P = 0x500000 + 0x20 + 0 - 0x400002 = 0x10001e
    assert_data(output_data, 0xff, 0x15, 0x1e, 0x00, 0x10, 0x00, END);              // call 0x10001e(%rip)
}

int test_R_X86_64_REX_GOTPCRELX() {
    // mov
    uint8_t output_data[] = {0x48, 0x8b, 0x0d, 0x00, 0x00, 0x00, 0x00};             // mov 0x0(%rip), %rcx
    reset(OT_STATIC_EXEC, output_data, R_X86_64_REX_GOTPCRELX, 3, 0x10);
    set_value(0x401000);
    run();
    assert_data(output_data, 0x48, 0xc7, 0xc1, 0x00, 0x10, 0x40, 0x00, END);        // mov $0x401000, %rcx

    uint8_t output_data2[] = {0x4c, 0x8b, 0x0d, 0x00, 0x00, 0x00, 0x00};            // mov 0x0(%rip), %r9
    reset(OT_STATIC_EXEC, output_data2, R_X86_64_REX_GOTPCRELX, 3, 0x10);
    set_value(0x401000);
    run();
    assert_data(output_data2, 0x49, 0xc7, 0xc1, 0x00, 0x10, 0x40, 0x00, END);       // mov $0x401000, %r9

    // cmp
    uint8_t output_data3[] = {0x4c, 0x8b, 0x0d, 0x00, 0x00, 0x00, 0x00};            // cmp 0x0(%rip), %rcx
    reset(OT_STATIC_EXEC, output_data3, R_X86_64_REX_GOTPCRELX, 3, 0x10);
    set_value(0x401000);
    run();
    assert_data(output_data3, 0x49, 0xc7, 0xc1, 0x00, 0x10, 0x40, 0x00, END);       // cmp $0x401000, %rcx

    uint8_t output_data4[] = {0x4c, 0x3b, 0x0d, 0x00, 0x00, 0x00, 0x00};            // cmp 0x0(%rip), %r9
    reset(OT_STATIC_EXEC, output_data4, R_X86_64_REX_GOTPCRELX, 3, 0x10);
    set_value(0x401000);
    run();
    assert_data(output_data4, 0x49, 0x81, 0xf9, 0x00, 0x10, 0x40, 0x00, END);       // cmp $0x401000, %r9

    // sub
    uint8_t output_data5[] = {0x48, 0x2b, 0x0d, 0x00, 0x00, 0x00, 0x00};            // sub 0x0(%rip), %rcx
    reset(OT_STATIC_EXEC, output_data5, R_X86_64_REX_GOTPCRELX, 3, 0x10);
    set_value(0x401000);
    run();
    assert_data(output_data5, 0x48, 0x81, 0xe9, 0x00, 0x10, 0x40, 0x00, END);       // sub $0x401000, %rcx

    uint8_t output_data6[] = {0x4c, 0x2b, 0x0d, 0x00, 0x00, 0x00, 0x00};            // sub 0x0(%rip), %r9
    reset(OT_STATIC_EXEC, output_data6, R_X86_64_REX_GOTPCRELX, 3, 0x10);
    set_value(0x401000);
    run();
    assert_data(output_data6, 0x49, 0x81, 0xe9, 0x00, 0x10, 0x40, 0x00, END);       // sub $0x401000, %r9
}

int test_R_X86_64_REX_GOTPCRELX_in_shared_object() {
    // S=401000 + A=10 - P=400003 = 0x100d
    uint8_t output_data1[] = {0x48, 0x8b, 0x0d, 0x00, 0x00, 0x00, 0x00};             // mov 0x0(%rip), %rcx
    reset(OT_DYN_EXEC, output_data1, R_X86_64_REX_GOTPCRELX, 3, 0x10);
    set_value(0x401000);
    run();
    assert_data(output_data1, 0x48, 0x8d, 0x0d, 0x0d, 0x10, 0x00, 0x00, END);        // lea 0x100d(%rip), %rcx
}

void test_R_X86_64_TPOFF32() {
    // With a TLS template size of 8, and symbol value of 0, the relative offset to the end of the TLS template is -8.
    // S + A - tls_template_tls_offset = 0 + 0 - 8
    uint8_t output_data[] = {0x00, 0x00, 0x00, 0x00};
    reset(OT_STATIC_EXEC, output_data, R_X86_64_TPOFF32, 0, 0);
    set_tls_template_tls_offset(8);
    set_plt_offset(8);
    run();
    assert_data(output_data, 0xf8, 0xff, 0xff, 0xff, END);
}

void test_R_X86_64_GOTPCREL_with_GOT_entry() {
    // This instruction cannot be relaxed, so a GOT entry has to be added for the symbol
    uint8_t output_data[] = {0x48, 0x83, 0x3d, 0xf8, 0x00, 0x00, 0x00, 0x00};      // cmpq $0, foo@GOTPCREL(%rip)
    reset(OT_STATIC_EXEC, output_data, R_X86_64_GOTPCREL, 4, 0);
    set_value(0x401000);
    set_got_offset(0x20);
    run();
    assert_data(output_data, 0x48, 0x83, 0x3d, 0xf8, 0x1c, 0x00, 0x10, 0x00, END); // 0x500000 - 0x400004 + 0x20 = 0x0010001c
}

void test_R_X86_64_GOTPCREL_with_GOT_iplt_entry() {
    // This instruction references something in the .got.iplt.
    uint8_t output_data[] = {0x48, 0x83, 0x3d, 0xf8, 0x00, 0x00, 0x00, 0x00};      // cmpq $0, foo@GOTPCREL(%rip)
    reset(OT_STATIC_EXEC, output_data, R_X86_64_GOTPCREL, 4, 0);
    set_value(0x401000);
    set_got_iplt_offset(0x20);
    run();
    assert_data(output_data, 0x48, 0x83, 0x3d, 0xf8, 0x1c, 0x00, 0x30, 0x00, END); // 0x700000 - 0x400004 + 0x20 = 0x0010001c
}

void test_R_X86_64_PLT32_in_static(void) {
    // For entries not in the PLT, R_X86_64_PLT32 behaves like a R_X86_64_PC32
    // S + A - P = 0x401000 + 0x10 - 0x400008 = 0x1008
    uint32_t output_data[4]; memset(output_data, -1, 16);
    reset(OT_STATIC_EXEC, output_data, R_X86_64_PLT32, 8, 0x10);
    set_value(0x401000);
    run();

    assert_uint32(-1, (output_data)[1], "R_X86_64_PLT32");
    assert_uint32(0x1008, (output_data)[2], "R_X86_64_PLT32");
    assert_uint32(-1, (output_data)[3], "R_X86_64_PLT32");

    // The symbol value is in the .iplt section
    // iplt_address + iplt_offset + A - P = 0x600000 + 0 + 0x20 - 0x400001 = 0x20001F
    uint8_t output_data2[] = {0xe8, 0x00, 0x00, 0x00, 0x00};      // callq foo@PLT and callq foo
    reset(OT_STATIC_EXEC, output_data2, R_X86_64_PLT32, 1, 0);
    set_value(0x401000);
    set_iplt_offset(0x20);
    run();
    assert_data(output_data2, 0xe8, 0x1f, 0x00, 0x20, 0x00, END);
}

// ET_DYN outputs are either executables or shared libraries.
// Symbols in the PLT gets statically linked in executables and dynamically linked in shared libraries
void test_R_X86_64_PLT32_in_shared(void) {
    // When linking dynamically, the symbol value is in the .plt section
    uint8_t output_data1[] = {0xe8, 0x00, 0x00, 0x00, 0x00};      // callq foo@PLT and callq foo
    reset(OT_SHLIB, output_data1, R_X86_64_PLT32, 1, 0);
    set_output_section_address(0);
    set_value(0x10);
    set_plt_offset(0x20);
    run();
    assert_data(output_data1, 0xe8, 0x1f, 0x80, 0x00, 0x00, END); // S(0x801f) = plt_offset(0x8000) + value_plt_offset(0x20) + A(0) - P(1);

    // When linking statically, R_X86_64_PLT32 is converted to R_X86_64_PC32
    // S + A - P = 0x401000 + 0 - 0x400001 = 0xfff
    uint8_t output_data2[] = {0xe8, 0x00, 0x00, 0x00, 0x00};      // callq foo@PLT and callq foo
    reset(OT_STATIC_EXEC, output_data2, R_X86_64_PLT32, 1, 0);
    set_value(0x401000);
    set_plt_offset(-1); // wtf, really?
    run();
    assert_data(output_data2, 0xe8, 0xff, 0x0f, 0x00, 0x00, END);
}

int main() {
    test_R_X86_64_64();
    test_R_X86_64_PC32();
    test_R_X86_64_32s(R_X86_64_32);
    test_R_X86_64_32s(R_X86_64_32S);
    test_R_X86_64_GOTPCRELX();
    test_R_X86_64_GOTPCRELX_in_shared_object();
    test_R_X86_64_REX_GOTPCRELX();
    test_R_X86_64_REX_GOTPCRELX_in_shared_object();
    test_R_X86_64_TPOFF32();
    test_R_X86_64_GOTPCREL_with_GOT_entry();
    test_R_X86_64_GOTPCREL_with_GOT_iplt_entry();
    test_R_X86_64_PLT32_in_static();
    test_R_X86_64_PLT32_in_shared();
}
