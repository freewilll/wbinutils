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

static int test_comments(void) {
    run("/* foo */");
    assert_int(linker_script->length, 0, "Comments");
}

static int test_entry(void) {
    run("ENTRY(foo)");
    assert_int(linker_script->length, 1, "ENTRY(foo)");
    ScriptCommand *command = linker_script->elements[0];
    assert_int(command->type, CMD_ENTRY, "ENTRY(foo)");
    assert_string(command->entry.symbol, "foo", "ENTRY(foo)");
}

static int test_double_entry(void) {
    run("ENTRY(foo);ENTRY(bar)");
    assert_int(linker_script->length, 2, "ENTRY(foo);ENTRY(bar)");

    ScriptCommand *command = linker_script->elements[0];
    assert_int(command->type, CMD_ENTRY, "ENTRY(foo);ENTRY(bar)");
    assert_string(command->entry.symbol, "foo", "ENTRY(foo);ENTRY(bar)");

    command = linker_script->elements[1];
    assert_int(command->type, CMD_ENTRY, "ENTRY(foo);ENTRY(bar)");
    assert_string(command->entry.symbol, "bar", "ENTRY(foo);ENTRY(bar)");
}

int main() {
    test_comments();
    test_entry();
    test_double_entry();
}
