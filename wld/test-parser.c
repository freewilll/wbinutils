#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wld/lexer.h"
#include "wld/parser.h"
#include "wld/script.h"

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
    char *script = "SECTIONS { a = 1 }";
    run(script);
    assert_int(1, linker_script->length, script);
    ScriptCommand *command = linker_script->elements[0];
    assert_int(CMD_SECTIONS, command->type, script);
    List *sections_commands = command->sections.commands;
    assert_int(1, sections_commands->length, script);
    SectionsCommand *section_command = sections_commands->elements[0];
    assert_int(SECTIONS_CMD_ASSIGNMENT, section_command->type, script);
}

void test_sections_output() {
    char *script = "SECTIONS { .text : { *(.text .text.*) x*(.xtext .xtext.*) } }";
    run(script);
    assert_int(1, linker_script->length, script);
    ScriptCommand *command = linker_script->elements[0];
    assert_int(CMD_SECTIONS, command->type, script);
    List *sections_commands = command->sections.commands;
    assert_int(1, sections_commands->length, script);

    SectionsCommand *section_command = sections_commands->elements[0];
    assert_int(SECTIONS_CMD_OUTPUT, section_command->type, script);
    assert_string(".text", section_command->output.output_section_name, script);

    List *input_sections = section_command->output.input_sections;
    assert_int(2, input_sections->length, script);

    InputSection *input_section;
    input_section = input_sections->elements[0];
    assert_string("*", input_section->file_pattern, script);
    assert_int(2, input_section->section_patterns->length, script);
    assert_string(".text", input_section->section_patterns->elements[0], script);
    assert_string(".text.*", input_section->section_patterns->elements[1], script);

    input_section = input_sections->elements[1];
    assert_string("x*", input_section->file_pattern, script);
    assert_int(2, input_section->section_patterns->length, script);
    assert_string(".xtext", input_section->section_patterns->elements[0], script);
    assert_string(".xtext.*", input_section->section_patterns->elements[1], script);
}

int main() {
    test_comments();
    test_entry();
    test_double_entry();
    test_sections_assignment();
    test_sections_output();
}
