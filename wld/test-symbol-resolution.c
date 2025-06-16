#include <stdlib.h>
#include <string.h>

#include "elf.h"
#include "list.h"
#include "input-elf.h"

#include "wld/symbols.h"
#include "wld/wld.h"

#define MAX_STRTAB_SIZE 1024

#define MGGS must_get_global_defined_symbol

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

static void run_init_symbols(void) {
    OutputElfFile *output_elf_file = init_output_elf_file("dummy", OUTPUT_TYPE_STATIC);
    init_symbols(output_elf_file);
}

static int add_string_to_strtab(InputElfFile *elf_file, const char *name) {
    if (elf_file->file_offset + strlen(name) + 1 > MAX_STRTAB_SIZE) {
        printf("Out of memory in MAX_STRTAB_SIZE");
        exit(1);
    }

    // Abuse file_offset to mean size of strtab strings
    int offset = elf_file->file_offset;
    memcpy(&elf_file->symbol_table_strings[elf_file->file_offset], name, strlen(name));
    elf_file->file_offset += strlen(name) + 1;

    return offset;
}

static ElfSymbol *add_symbol(InputElfFile *elf_file, const char *name, long size, int binding, int type, int section_index, int value) {
    elf_file->symbol_count++;
    elf_file->symbol_table = realloc(elf_file->symbol_table, elf_file->symbol_count * sizeof(ElfSymbol));

    ElfSymbol *symbol = &elf_file->symbol_table[elf_file->symbol_count - 1];
    memset(symbol, 0, sizeof(ElfSymbol));

    symbol->st_name = add_string_to_strtab(elf_file, name);
    symbol->st_size = size;
    symbol->st_info = (binding << 4) + type;
    symbol->st_shndx = section_index;
    symbol->st_value = value;

    return symbol;
}

InputElfFile *init_elf_file(void) {
    InputElfFile *elf_file = calloc(1, sizeof(InputElfFile));
    elf_file->filename = "/_testing.o";
    elf_file->symbol_table_strings = calloc(1, MAX_STRTAB_SIZE);
    add_string_to_strtab(elf_file, "");

    // Create two sections without data
    // The first is the NULL section, the second a fake section for the tests.
    elf_file->section_list = new_list(2);
    append_to_list(elf_file->section_list, NULL);
    append_to_list(elf_file->section_list, NULL);

    return elf_file;
}

// library values:
// 0 - is_library=0
// 1 - is_library=1
// 2 - auto: is_library=1, but run with both read_only=1 and read_only=0, like the real linker does
static int process_one_symbol(const char *name, int size, int binding, int type, int section_index, int value, int library, int read_only) {
    InputElfFile *elf_file = init_elf_file();
    add_symbol(elf_file, name, size, binding, type, section_index, value);

    int symbol_resolutions;

    if (library == 2) {
        // Only include the library if it resolves other symbols.
        symbol_resolutions = process_elf_file_symbols(elf_file, 1, 0 ,1);
        if (symbol_resolutions) process_elf_file_symbols(elf_file, 1, 0, 0);
    }
    else {
        process_elf_file_symbols(elf_file, library, 0, read_only);
    }

    return symbol_resolutions;
}

static void test_two_strong_symbols(void) {
    // Two strong symbols are allowed if the second one is a library.
    // In that case, the first symbol takes precedence

    // obj - obj is fail
    run_init_symbols();
    process_one_symbol("foo", 4, STB_GLOBAL, STT_OBJECT, 1, 1, 0, 0);
    assert_no_error("Two strong symbols in obj-obj");
    process_one_symbol("foo", 4, STB_GLOBAL, STT_OBJECT, 1, 1, 0, 0);
    assert_error("Multiple definition of foo", "Two strong symbols in obj-obj");

    // obj - lib is ok
    run_init_symbols();
    process_one_symbol("foo", 4, STB_GLOBAL, STT_OBJECT, 1, 1, 0, 0);
    assert_no_error("Two strong symbols in obj-lib");
    process_one_symbol("foo", 4, STB_GLOBAL, STT_OBJECT, 1, 1, 1, 0);
    assert_no_error("Two strong symbols in obj-lib");

    // lib - lib is ok
    run_init_symbols();
    process_one_symbol("foo", 4, STB_GLOBAL, STT_OBJECT, 1, 1, 1, 0);
    assert_no_error("Two strong symbols in lib-lib");
    process_one_symbol("foo", 4, STB_GLOBAL, STT_OBJECT, 1, 1, 1, 0);
    assert_no_error("Two strong symbols in lib-lib");
}

// Add three symbols of the same name:
// The first one is undefined and not in a lib
// The second one can be a object or lib and weak or string
// The third is like the second
static void _test_strong_and_weak_symbols(int binding1, int is_lib2, int binding2, int is_lib3, int binding3, int expected_value, const char *message) {
    run_init_symbols();

    // Add the first symbol. It's undefined and in an object
    process_one_symbol("foo", 4, binding1, STT_OBJECT, SHN_UNDEF, 0, 0, 0);

    // Add the second symbol
    process_one_symbol("foo", 4, binding2, STT_OBJECT, 1, 1, is_lib2, 0);
    assert_no_error(message);

    // Add the third symbol
    process_one_symbol("foo", 4, binding3, STT_OBJECT, 1, 2, is_lib3, 0);
    assert_no_error(message);

    if (expected_value != 0) {
        // By convention in this test, the value must be defined
        assert_int(expected_value, MGGS("foo")->src_value, message);
    }
    else {
        // By convention in this test, an expected_value of zero means the variable must be be undefined
        assert_int(1, is_undefined_symbol("foo"), message);
    }
}

static void test_strong_and_weak_symbols(void) {
    // Test combinations of weak and strong symbols + an initial undefined one.
    // A symbol in a library never resolves a weak symbol.

    _test_strong_and_weak_symbols(STB_GLOBAL, 0, STB_GLOBAL, 0, STB_WEAK,   1, "Three symbols with different bindings s - os - ow");
    _test_strong_and_weak_symbols(STB_GLOBAL, 0, STB_GLOBAL, 2, STB_WEAK,   1, "Three symbols with different bindings s - os - lw");
    _test_strong_and_weak_symbols(STB_GLOBAL, 2, STB_GLOBAL, 0, STB_WEAK,   1, "Three symbols with different bindings s - ls - ow");
    _test_strong_and_weak_symbols(STB_GLOBAL, 2, STB_GLOBAL, 2, STB_WEAK,   1, "Three symbols with different bindings s - ls - lw");
    _test_strong_and_weak_symbols(STB_GLOBAL, 0, STB_WEAK,   0, STB_GLOBAL, 2, "Three symbols with different bindings s - ow - os");
    _test_strong_and_weak_symbols(STB_GLOBAL, 0, STB_WEAK,   2, STB_GLOBAL, 1, "Three symbols with different bindings s - ow - ls");
    _test_strong_and_weak_symbols(STB_GLOBAL, 2, STB_WEAK,   0, STB_GLOBAL, 2, "Three symbols with different bindings s - lw - os");
    _test_strong_and_weak_symbols(STB_GLOBAL, 2, STB_WEAK,   2, STB_GLOBAL, 1, "Three symbols with different bindings s - lw - ls");

    _test_strong_and_weak_symbols(STB_WEAK,   0, STB_GLOBAL, 0, STB_WEAK,   1, "Three symbols with different bindings w - os - ow");
    _test_strong_and_weak_symbols(STB_WEAK,   0, STB_GLOBAL, 2, STB_WEAK,   1, "Three symbols with different bindings w - os - lw");
    _test_strong_and_weak_symbols(STB_WEAK,   2, STB_GLOBAL, 0, STB_WEAK,   2, "Three symbols with different bindings w - ls - ow");
    _test_strong_and_weak_symbols(STB_WEAK,   2, STB_GLOBAL, 2, STB_WEAK,   0, "Three symbols with different bindings w - ls - lw");
    _test_strong_and_weak_symbols(STB_WEAK,   0, STB_WEAK,   0, STB_GLOBAL, 2, "Three symbols with different bindings w - ow - os");
    _test_strong_and_weak_symbols(STB_WEAK,   0, STB_WEAK,   2, STB_GLOBAL, 1, "Three symbols with different bindings w - ow - ls");
    _test_strong_and_weak_symbols(STB_WEAK,   2, STB_WEAK,   0, STB_GLOBAL, 2, "Three symbols with different bindings w - lw - os");
    _test_strong_and_weak_symbols(STB_WEAK,   2, STB_WEAK,   2, STB_GLOBAL, 0, "Three symbols with different bindings w - lw - ls");
}

static void test_common_symbols(void) {
    // One common symbol
    run_init_symbols();
    process_one_symbol("foo", 4, STB_GLOBAL, STT_OBJECT, SHN_COMMON, 1, 0, 0);
    assert_no_error("One common");
    assert_int(1, MGGS("foo")->is_common, "One common");

    // Undefined, then a common symbol
    run_init_symbols();
    process_one_symbol("foo", 4, STB_GLOBAL, STT_OBJECT, SHN_UNDEF, 1, 0, 0);
    assert_no_error("Undef then common");
    assert_int(1, is_undefined_symbol("foo"), "Undef then common");
    process_one_symbol("foo", 4, STB_GLOBAL, STT_OBJECT, SHN_COMMON, 1, 0, 0);
    assert_no_error("Undef then common");
    assert_int(0, is_undefined_symbol("foo"), "Undef then common");

    // Defined, then a common symbol
    run_init_symbols();
    process_one_symbol("foo", 4, STB_GLOBAL, STT_OBJECT, 1, 1, 0, 0);
    assert_no_error("Defined, then common");
    assert_int(1, MGGS("foo")->src_value, "Defined, then common");
    process_one_symbol("foo", 4, STB_GLOBAL, STT_OBJECT, SHN_COMMON, 1, 0, 0);
    assert_int(1, MGGS("foo")->src_value, "Defined, then common");

    // Common symbol, then defined
    run_init_symbols();
    process_one_symbol("foo", 4, STB_GLOBAL, STT_OBJECT, SHN_COMMON, 1, 0, 0);
    assert_no_error("Common then defined");
    assert_int(1, MGGS("foo")->src_value, "Common then defined");
    assert_int(1, MGGS("foo")->is_common, "Common then defined");
    process_one_symbol("foo", 4, STB_GLOBAL, STT_OBJECT, 1, 2, 0, 0);
    assert_int(0, must_get_global_defined_symbol("foo")->is_common, "Common then defined");
    assert_int(2, must_get_global_defined_symbol("foo")->src_value, "Common then defined");
}

// Add two undefined symbols, the first weak, the second strong. The resulting undefined symbol must be strong.
// This caused a bug where C-ctype.o in glibc wasn't being included, leading to a crash in __ctype_init.
static void test_two_undefined_symbols_one_weak_one_strong(void) {
    run_init_symbols();

    process_one_symbol("foo", 4, STB_WEAK, STT_OBJECT, SHN_UNDEF, 1, 0, 0);
    process_one_symbol("foo", 4, STB_GLOBAL, STT_OBJECT, SHN_UNDEF, 1, 0, 0);

    Symbol *undefined_symbol = get_undefined_symbol("foo");
    if (undefined_symbol->binding != STB_GLOBAL) {
        printf("Expected a weak binding to be upgraded to a strong one\n");
        exit(1);
    }
}

static void test_two_defined_symbols_one_weak_one_strong(void) {
    // A strong symbol in a library doesn't get it to be pulled in if a weak symbol already is present
    run_init_symbols();

    // Add an undefined symbol so that the library gets pulled in
    process_one_symbol("foo", 4, STB_GLOBAL, STT_OBJECT, SHN_UNDEF, 1, 0, 0);

    // Add a library with a weak foo that resolves foo
    int resolutions = process_one_symbol("foo", 4, STB_WEAK,   STT_OBJECT, 1, 1, 2, 0);
    assert_int(1, resolutions, "Expected the weak symbol to resolve foo");

    // Add another library with a strong foo. It is ignored
    resolutions = process_one_symbol("foo", 4, STB_GLOBAL, STT_OBJECT, 1, 1, 2, 0);
    assert_int(0, resolutions, "Expected the strong symbol to be ignored");

    // The weak symbol resolves foo
    Symbol *symbol = must_get_defined_symbol(global_symbol_table, "foo");
    assert_int(STB_WEAK, symbol->binding, "Expected the weak symbol to not be overridden by the strong one");

    // If an object file in a library is loaded, and has a strong symbol, it overrides an already
    // resolved weak symbol.
    run_init_symbols();

    // Add an undefined symbol so that the library gets pulled in
    process_one_symbol("foo", 4, STB_GLOBAL, STT_OBJECT, SHN_UNDEF, 1, 0, 0);

    // Add a library with a weak foo that resolves foo
    resolutions = process_one_symbol("foo", 4, STB_WEAK,   STT_OBJECT, 1, 1, 2, 0);
    assert_int(1, resolutions, "Expected the weak symbol to resolve foo");

    // Load a library with a strong foo. foo becomes strong
    resolutions = process_one_symbol("foo", 4, STB_GLOBAL, STT_OBJECT, 1, 1, 1, 0);
    assert_int(1, resolutions, "The strong symbol takes over the weak");

    // The strong symbol overrides the weak one
    symbol = must_get_defined_symbol(global_symbol_table, "foo");
    assert_int(STB_GLOBAL, symbol->binding, "Expected the strong symbol to override the weak one");
}

int main() {
    test_two_strong_symbols();
    test_strong_and_weak_symbols();
    test_common_symbols();
    test_two_undefined_symbols_one_weak_one_strong();
    test_two_defined_symbols_one_weak_one_strong();
}
