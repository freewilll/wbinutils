#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elf.h"
#include "output-elf.h"

#include "wld/lexer.h"
#include "wld/expr.h"
#include "wld/script.h"
#include "wld/symbols.h"
#include "wld/wld.h"

static void assert_int(int expected, int actual, char *message) {
    if (expected != actual) {
        printf("%s: expected %d, got %d\n", message, expected, actual);
        exit(1);
    }
}

static void assert_string(const char *expected, const char *actual, const char *message) {
    if (strcmp(expected, actual)) {
        printf("%s: expected %s, got %s\n", message, expected, actual);
        exit(1);
    }
}

static void run_init_symbols(void) {
    OutputElfFile *output_elf_file = init_output_elf_file("dummy", OUTPUT_TYPE_FLAG_STATIC);
    init_symbols(output_elf_file);
}

static Value run_eval(char *script) {
    init_lexer_from_string(script);

    OutputElfFile *output_elf_file = new_output_elf_file("", ET_EXEC);

    return evaluate_node(parse_expression(), output_elf_file);
}

static void test_constant() {
    Value value;

    value = run_eval("1");    assert_int(1,    value.number, "1");
    value = run_eval("1000"); assert_int(1000, value.number, "1000");
    value = run_eval("0x10"); assert_int(16,   value.number, "0x10");
    value = run_eval("0100"); assert_int(64,   value.number, "0100");

    value = run_eval("CONSTANT(MAXPAGESIZE)");    assert_int(0x1000, value.number, "CONSTANT(MAXPAGESIZE)");
    value = run_eval("CONSTANT(COMMONPAGESIZE)"); assert_int(0x1000, value.number, "CONSTANT(COMMONPAGESIZE)");

    value = run_eval("1 + 2");       assert_int(3,  value.number, "1 + 2");
    value = run_eval("3 - 1");       assert_int(2,  value.number, "3 - 1");
    value = run_eval("3 * 2");       assert_int(6,  value.number, "3 * 2");
    value = run_eval("6 / 2");       assert_int(3,  value.number, "6 / 2");
    value = run_eval("6 * (1 + 2)"); assert_int(18, value.number, "6 * (1 + 2)");
    value = run_eval("6 * 2 + 1");   assert_int(13, value.number, "6 * 2 + 1");
    value = run_eval("6 + 2 * 3");   assert_int(12, value.number, "6 + 2 * 3");
    value = run_eval("1 == 1");      assert_int(1,  value.number, "1 == 1");
    value = run_eval("1 == 2");      assert_int(0,  value.number, "1 == 2");
    value = run_eval("1 != 1");      assert_int(0,  value.number, "1 != 1");
    value = run_eval("1 != 2");      assert_int(1,  value.number, "1 != 2");
    value = run_eval("1 <  2");      assert_int(1,  value.number, "1 <  2");
    value = run_eval("1 >  2");      assert_int(0,  value.number, "1 >  2");
    value = run_eval("1 <= 2");      assert_int(1,  value.number, "1 <= 2");
    value = run_eval("1 >= 2");      assert_int(0,  value.number, "1 >= 2");
    value = run_eval("2 <  1");      assert_int(0,  value.number, "2 <  1");
    value = run_eval("2 >  1");      assert_int(1,  value.number, "2 >  1");
    value = run_eval("2 <= 1");      assert_int(0,  value.number, "2 <= 1");
    value = run_eval("2 >= 1");      assert_int(1,  value.number, "2 >= 1");
    value = run_eval("1 <  1");      assert_int(0,  value.number, "2 <  1");
    value = run_eval("1 >  1");      assert_int(0,  value.number, "2 >  1");
    value = run_eval("1 <= 1");      assert_int(1,  value.number, "2 <= 1");
    value = run_eval("1 >= 1");      assert_int(1,  value.number, "2 >= 1");
    value = run_eval("2 * 4 > 7");   assert_int(1,  value.number, "2 * 4 > 7");
    value = run_eval("2 * 4 == 8");  assert_int(1,  value.number, "2 * 4 == 8");
    value = run_eval("0 ? 2 : 3");   assert_int(3,  value.number, "0 ? 2 : 3");
    value = run_eval("1 ? 2 : 3");   assert_int(2,  value.number, "1 ? 2 : 3");
}

static Symbol *make_or_get_dot_symbol(void) {
    CommandAssignment dot_assignment = (CommandAssignment) {.name = "."};
    return get_or_add_linker_script_symbol(&dot_assignment);
}

static void test_symbol() {
    Value value;

    run_init_symbols();

    // foo
    value = run_eval("foo");
    assert_int(0, value.symbol->dst_value, "foo");
    must_get_global_defined_symbol("foo", 0)->dst_value = 1;
    assert_int(1, value.symbol->dst_value, "foo");

    // .
    make_or_get_dot_symbol()->dst_value = 0x400000;
    value = run_eval(".");
    assert_int(0x400000, value.symbol->dst_value, ".");
}

static void test_expressions_with_symbol() {
    Value value;

    run_init_symbols();
    make_or_get_dot_symbol()->dst_value = 0x400000;

    // Expressions
    value = run_eval(". + 0x1000");             assert_int(0x401000, value.number,  ". + 0x1000");
    value = run_eval(". * 2 + 0x1000");         assert_int(0x801000, value.number,  ". * 2 + 0x1000");
    value = run_eval(". == 0x400000");          assert_int(1,        value.number,  ". == 0x400000");
    value = run_eval(". != 0x400000");          assert_int(0,        value.number,  ". != 0x400000");
    value = run_eval(". > 0x300000");           assert_int(1,        value.number,  ". > 0x300000");
    value = run_eval(". < 0x300000");           assert_int(0,        value.number,  ". < 0x300000");
    value = run_eval(". >= 0x400000");          assert_int(1,        value.number,  ". >= 0x400000");
    value = run_eval(". <= 0x400000");          assert_int(1,        value.number,  ". <= 0x400000");
    value = run_eval(". == 0x400000 ? 1 : 2");  assert_int(1,        value.number,  ". == 0x400000 ? 1 : 2");
    value = run_eval(". >  0x500000 ? 1 : 2");  assert_int(2,        value.number,  ". > 0x500000 ? 1 : 2");

    // ALIGN()
    make_or_get_dot_symbol()->dst_value = 0x1800;
    value = run_eval("ALIGN(0x1000)");          assert_int(0x2000, value.number,  "ALIGN(0x1000)");
    value = run_eval("ALIGN(0x0fff, 0x1000)");  assert_int(0x1000, value.number,  "ALIGN(0x0fff, 0x1000)");
    value = run_eval("ALIGN(0x1000, 0x1000)");  assert_int(0x1000, value.number,  "ALIGN(0x1000, 0x1000)");
    value = run_eval("ALIGN(0x1001, 0x1000)");  assert_int(0x2000, value.number,  "ALIGN(0x1001, 0x1000)");
    value = run_eval("ALIGN(7, 6)");            assert_int(12,     value.number,  "ALIGN(7, 6)");

}

void test_sizeof() {
    char *script = "SIZEOF(.text)";
    init_lexer_from_string(script);

    OutputElfFile *output_elf_file = new_output_elf_file("", ET_EXEC);
    OutputSection *section = add_output_section(output_elf_file, ".text" , SHT_PROGBITS, 0, 0x1000);
    section->size = 0x100;

    Value value = evaluate_node(parse_expression(), output_elf_file);
    assert_int(0x100, value.number, script);
}

void test_sizeof_headers() {
    char *script = "SIZEOF_HEADERS";
    init_lexer_from_string(script);

    OutputElfFile *output_elf_file = new_output_elf_file("", ET_EXEC);
    output_elf_file->elf_program_segments_header_size = 0x100;
    output_elf_file->elf_section_headers_size = 0x200;

    Value value = evaluate_node(parse_expression(), output_elf_file);
    assert_int(sizeof(ElfHeader) + 0x300, value.number, script);
}

int main() {
    test_constant();
    test_symbol();
    test_expressions_with_symbol();
    test_sizeof();
    test_sizeof_headers();
}