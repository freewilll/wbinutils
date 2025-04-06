#include <stdlib.h>
#include <string.h>

#include "elf.h"
#include "list.h"
#include "ro-elf.h"

#include "wld/symbols.h"

#define MAX_STRTAB_SIZE 1024

static void assert_no_error(const char *message) {
    if (last_error_message) {
        printf("%s: expected no error, got  %s\n", message, last_error_message);
        exit(1);
    }
}

static void assert_error(const char *expected, const char *message) {
    if (!last_error_message) {
        printf("%s: expected %s, got none\n", message, expected);
        exit(1);
    }

    if (strcmp(last_error_message, expected)) {
        printf("%s: expected %s, got %s\n", message, expected, last_error_message);
    }
}

static void assert_int(int expected, int actual, const char *message) {
    if (expected != actual) {
        printf("%s: expected %d, got %d\n", message, expected, actual);
        exit(1);
    }
}

static int add_string_to_strtab(ElfFile *elf_file, const char *name) {
    if (elf_file->file_offset + strlen(name) + 1 > MAX_STRTAB_SIZE) {
        printf("Out of memory in MAX_STRTAB_SIZE");
        exit(1);
    }

    // Abuse file_offset to mean size of strtab strings
    int offset = elf_file->file_offset;
    memcpy(&elf_file->strtab_strings[elf_file->file_offset], name, strlen(name));
    elf_file->file_offset += strlen(name) + 1;

    return offset;
}

static ElfSymbol *add_symbol(ElfFile *elf_file, const char *name, long size, int binding, int type, int value) {
    elf_file->symbol_count++;
    elf_file->symbol_table = realloc(elf_file->symbol_table, elf_file->symbol_count * sizeof(ElfSymbol));

    ElfSymbol *symbol = &elf_file->symbol_table[elf_file->symbol_count - 1];
    memset(symbol, 0, sizeof(ElfSymbol));

    symbol->st_name = add_string_to_strtab(elf_file, name);
    symbol->st_size = size;
    symbol->st_info = (binding << 4) + type;
    symbol->st_shndx = 1;
    symbol->st_value = value;

    return symbol;
}

ElfFile *init_elf_file(void) {
    ElfFile *elf_file = calloc(1, sizeof(ElfFile));
    elf_file->filename = "/_testing.o";
    elf_file->strtab_strings = calloc(1, MAX_STRTAB_SIZE);
    add_string_to_strtab(elf_file, "");

    // Create one section without data
    elf_file->section_list = new_list(1);
    append_to_list(elf_file->section_list, NULL);

    return elf_file;
}

static void process_one_symbol(const char *name, int size, int binding, int type, int value, int is_library) {
    ElfFile *elf_file = init_elf_file();
    add_symbol(elf_file, name, size, binding, type, value);
    process_elf_file_symbols(elf_file, is_library, 0);
}

void test_two_strong_symbols(void) {
    // Two strong symbols are allowed if the second one is a library.
    // In that case, the first symbol takes precedence

    // obj - obj is fail
    init_symbols();
    process_one_symbol("foo", 4, STB_GLOBAL, STT_OBJECT, 1, 0);
    assert_no_error("Two strong symbols in obj-obj");
    process_one_symbol("foo", 4, STB_GLOBAL, STT_OBJECT, 1, 0);
    assert_error("Multiple definition of foo", "Two strong symbols in obj-obj");

    // obj - lib is ok
    init_symbols();
    process_one_symbol("foo", 4, STB_GLOBAL, STT_OBJECT, 1, 0);
    assert_no_error("Two strong symbols in obj-lib");
    process_one_symbol("foo", 4, STB_GLOBAL, STT_OBJECT, 1, 1);
    assert_no_error("Two strong symbols in obj-lib");

    // lib - lib is ok
    init_symbols();
    process_one_symbol("foo", 4, STB_GLOBAL, STT_OBJECT, 1, 1);
    assert_no_error("Two strong symbols in lib-lib");
    process_one_symbol("foo", 4, STB_GLOBAL, STT_OBJECT, 1, 1);
    assert_no_error("Two strong symbols in lib-lib");
}

void _test_two_weak_symbols(int left_is_lib, int right_is_lib, const char *message) {
    init_symbols();
    process_one_symbol("foo", 4, STB_WEAK, STT_OBJECT, 1, left_is_lib);
    assert_no_error(message);
    process_one_symbol("foo", 4, STB_WEAK, STT_OBJECT, 2, right_is_lib);
    assert_no_error(message);
    assert_int(1, must_get_defined_symbol("foo")->src_value, message);
}

void test_two_weak_symbols(void) {
    // Two weak symbols are always allowed. The first one takes precedence

    _test_two_weak_symbols(0, 0, "Two weak symbols in obj-obj");
    _test_two_weak_symbols(0, 1, "Two weak symbols in obj-lib");
    _test_two_weak_symbols(1, 0, "Two weak symbols in lib-obj");
    _test_two_weak_symbols(1, 1, "Two weak symbols in lib-obj");
}

void _test_strong_and_weak_symbols(int left_is_lib, int right_is_lib, int left_binding, int right_binding, int expected_value, const char *message) {
    init_symbols();
    process_one_symbol("foo", 4, left_binding, STT_OBJECT, 1, left_is_lib);
    assert_no_error(message);
    process_one_symbol("foo", 4, right_binding, STT_OBJECT, 2, right_is_lib);
    assert_no_error(message);
    assert_int(expected_value, must_get_defined_symbol("foo")->src_value, message);
}

void test_strong_and_weak_symbols(void) {
    // A strong symbol always takes precedence over a weak one. The order doesn't matter.

    _test_strong_and_weak_symbols(0, 0, STB_GLOBAL, STB_WEAK, 1, "Two strong-weak symbols in obj-obj oo 1");
    _test_strong_and_weak_symbols(0, 1, STB_GLOBAL, STB_WEAK, 1, "Two strong-weak symbols in obj-obj ol 1");
    _test_strong_and_weak_symbols(1, 0, STB_GLOBAL, STB_WEAK, 1, "Two strong-weak symbols in obj-obj lo 1");
    _test_strong_and_weak_symbols(1, 1, STB_GLOBAL, STB_WEAK, 1, "Two strong-weak symbols in obj-obj ll 1");

    _test_strong_and_weak_symbols(0, 0, STB_WEAK, STB_GLOBAL, 2, "Two strong-weak symbols in obj-obj oo 2");
    _test_strong_and_weak_symbols(0, 1, STB_WEAK, STB_GLOBAL, 2, "Two strong-weak symbols in obj-obj ol 2");
    _test_strong_and_weak_symbols(1, 0, STB_WEAK, STB_GLOBAL, 2, "Two strong-weak symbols in obj-obj lo 2");
    _test_strong_and_weak_symbols(1, 1, STB_WEAK, STB_GLOBAL, 2, "Two strong-weak symbols in obj-obj ll 2");
}

int main() {
    test_two_strong_symbols();
    test_two_weak_symbols();
    test_strong_and_weak_symbols();
}
