#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wld/expr.h"
#include "wld/lexer.h"
#include "wld/parser.h"
#include "wld/script.h"
#include "wld/symbols.h"

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

static void run(char *script) {
    linker_script = new_list(1);
    init_lexer_from_string(script);
    parse();
}

static uint64_t evaluate_test_node(Node *node) {
    RwElfFile *rw_elf_file = new_rw_elf_file("", ET_EXEC);
    Value value = evaluate_node(node, rw_elf_file);
    return value.number;
}

static void test_comments(void) {
    run("/* foo */");
    assert_int(linker_script->length, 0, "Comments");
}

static void test_entry(void) {
    run("ENTRY(foo)");
    assert_int(linker_script->length, 1, "ENTRY(foo)");
    ScriptCommand *command = linker_script->elements[0];
    assert_int(CMD_ENTRY, command->type, "ENTRY(foo)");
    assert_string("foo", command->entry.symbol, "ENTRY(foo)");
}

static void test_double_entry(void) {
    run("ENTRY(foo);ENTRY(bar)");
    assert_int(2, linker_script->length, "ENTRY(foo);ENTRY(bar)");

    ScriptCommand *command = linker_script->elements[0];
    assert_int(CMD_ENTRY, command->type, "ENTRY(foo);ENTRY(bar)");
    assert_string("foo", command->entry.symbol, "ENTRY(foo);ENTRY(bar)");

    command = linker_script->elements[1];
    assert_int(CMD_ENTRY, command->type, "ENTRY(foo);ENTRY(bar)");
    assert_string("bar", command->entry.symbol, "ENTRY(foo);ENTRY(bar)");
}

void test_sections_assignment() {
    char *script =
        "SECTIONS {\n"
        "    a = 1\n"
        "    PROVIDE(b = 2)\n"
        "    PROVIDE_HIDDEN(c = 3)\n"
        "}";

    run(script);
    assert_int(1, linker_script->length, script);
    ScriptCommand *command = linker_script->elements[0];
    assert_int(CMD_SECTIONS, command->type, script);

    List *sections_commands = command->sections.commands;
    assert_int(3, sections_commands->length, script);

    // a = 1
    SectionsCommand *section_command = sections_commands->elements[0];
    assert_int(SECTIONS_CMD_ASSIGNMENT, section_command->type, script);
    assert_string("a", section_command->assignment.name, script);
    assert_int(1, evaluate_test_node(section_command->assignment.node), script);

    // PROVIDE(b = 2)
    section_command = sections_commands->elements[1];
    assert_int(SECTIONS_CMD_ASSIGNMENT, section_command->type, script);
    assert_string("b", section_command->assignment.name, script);
    assert_int(1, section_command->assignment.provide, script);
    assert_int(2, evaluate_test_node(section_command->assignment.node), script);

    // PROVIDE_HIDDEN(c = 3)
    section_command = sections_commands->elements[2];
    assert_int(SECTIONS_CMD_ASSIGNMENT, section_command->type, script);
    assert_string("c", section_command->assignment.name, script);
    assert_int(1, section_command->assignment.provide_hidden, script);
    assert_int(3, evaluate_test_node(section_command->assignment.node), script);
}

void test_sections_output() {
    char *script =
        "SECTIONS {\n"
        "   .text : { *(.text .text.*) x*(.xtext .xtext.*) }\n"
        "   .keeper : { KEEP(y(.keeper)) }\n"
        "   .assignment_in_section : { \n"
        "       a = 1;\n"
        "       PROVIDE(b = 2);\n"
        "       PROVIDE_HIDDEN(c = 3);\n"
        "    }\n"
        "}\n";

    run(script);
    assert_int(1, linker_script->length, script);
    ScriptCommand *command = linker_script->elements[0];
    assert_int(CMD_SECTIONS, command->type, script);

    List *sections_commands = command->sections.commands;
    assert_int(3, sections_commands->length, script);

    // .text
    SectionsCommand *section_command = sections_commands->elements[0];
    assert_int(SECTIONS_CMD_OUTPUT, section_command->type, script);
    assert_string(".text", section_command->output.output_section_name, script);

    List *output_items = section_command->output.output_items;
    assert_int(2, output_items->length, script);

    SectionsCommandOutputItem *output_item = output_items->elements[0];
    assert_int(SECTIONS_CMD_INPUT_SECTION, output_item->type, script);
    InputSection *input_section = &output_item->input_section;
    assert_string("*", input_section->file_pattern, script);
    assert_int(2, input_section->section_patterns->length, script);
    assert_string(".text", input_section->section_patterns->elements[0], script);
    assert_string(".text.*", input_section->section_patterns->elements[1], script);

    output_item = output_items->elements[1];
    assert_int(SECTIONS_CMD_INPUT_SECTION, output_item->type, script );
    input_section = &output_item->input_section;
    assert_string("x*", input_section->file_pattern, script);
    assert_int(2, input_section->section_patterns->length, script);
    assert_string(".xtext", input_section->section_patterns->elements[0], script);
    assert_string(".xtext.*", input_section->section_patterns->elements[1], script);

    // .keeper
    section_command = sections_commands->elements[1];
    assert_int(SECTIONS_CMD_OUTPUT, section_command->type, script);
    assert_string(".keeper", section_command->output.output_section_name, script);

    output_items = section_command->output.output_items;
    assert_int(1, output_items->length, script);

    output_item = output_items->elements[0];
    assert_int(SECTIONS_CMD_INPUT_SECTION, output_item->type, script);
    input_section = &output_item->input_section;
    assert_int(1, input_section->keep, script);
    assert_string("y", input_section->file_pattern, script);
    assert_int(1, input_section->section_patterns->length, script);
    assert_string(".keeper", input_section->section_patterns->elements[0], script);

    // .assignment_in_section
    section_command = sections_commands->elements[2];
    assert_int(SECTIONS_CMD_OUTPUT, section_command->type, script);
    assert_string(".assignment_in_section", section_command->output.output_section_name, script);

    output_items = section_command->output.output_items;
    assert_int(3, output_items->length, script);

    // .assignment_in_section a = 1
    output_item = output_items->elements[0];
    assert_int(SECTIONS_CMD_INPUT_ASSIGNMENT, output_item->type, script);
    CommandAssignment assignment = output_item->assignment;
    assert_string("a", assignment.name, script);
    assert_int(1, evaluate_test_node(assignment.node), script);

    // .assignment_in_section PROVIDE(b = 2)
    output_item = output_items->elements[1];
    assert_int(SECTIONS_CMD_INPUT_ASSIGNMENT, output_item->type, script);
    assignment = output_item->assignment;
    assert_string("b", assignment.name, script);
    assert_int(1, assignment.provide, script);
    assert_int(2, evaluate_test_node(assignment.node), script);

    // .assignment_in_section PROVIDE_HIDDEN(c = 3)
    output_item = output_items->elements[2];
    assert_int(SECTIONS_CMD_INPUT_ASSIGNMENT, output_item->type, script);
    assignment = output_item->assignment;
    assert_string("c", assignment.name, script);
    assert_int(1, assignment.provide_hidden, script);
    assert_int(3, evaluate_test_node(assignment.node), script);
}

static void test_parse_default_linker_script() {
    init_symbols();
    run(DEFAULT_LINKER_SCRIPT);
}

int main() {
    test_comments();
    test_entry();
    test_double_entry();
    test_sections_assignment();
    test_sections_output();
    test_parse_default_linker_script();
}
