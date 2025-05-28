#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wld/utils.h"

#define assert_path_match(string, pattern) assert_path_pattern_match(string, pattern, 1)
#define assert_path_not_match(string, pattern) assert_path_pattern_match(string, pattern, 0)

static void assert_pattern_match(char *string, char *pattern, int expected_matches) {
    int matches = match_pattern(string, pattern);

    if (matches && !expected_matches) {
        printf("Got unexpected match for %s to %s\n", string, pattern);
        exit(1);
    }

    if (!matches && expected_matches) {
        printf("Did not match %s to %s\n", string, pattern);
        exit(1);
    }
}

#define assert_match(string, pattern) assert_pattern_match(string, pattern, 1)
#define assert_not_match(string, pattern) assert_pattern_match(string, pattern, 0)

static void assert_path_pattern_match(char *string, char *pattern, int expected_matches) {
    int matches = match_path_pattern(string, pattern);

    if (matches && !expected_matches) {
        printf("Got unexpected match for %s to %s\n", string, pattern);
        exit(1);
    }

    if (!matches && expected_matches) {
        printf("Did not match %s to %s\n", string, pattern);
        exit(1);
    }
}

static void assert_is_c_identifier(const char *identifier, int expected_value) {
    int got_value = is_c_identifier(identifier);
    if (got_value != expected_value) {
        printf("is_c_identifier for %s was expected to be %d\n", identifier, expected_value);
        exit(1);
    }
}

int main() {
    assert_not_match("foo", "bar");
    assert_not_match("", "x");
    assert_not_match("x", "");
    assert_match("", "");
    assert_match("f", "f");
    assert_not_match("f", "fo");
    assert_match("foo", "foo");
    assert_match("foo", "fo*");
    assert_match("foo", "foo*");
    assert_match("foo", "*oo");
    assert_match("foo", "*foo");
    assert_not_match("foo", "*b");
    assert_match("foo", "f*o");
    assert_match("foo", "f*");
    assert_not_match("foo", "f*b");
    assert_not_match("foo", "f?b");
    assert_match("foo", "f?o");
    assert_match("fxo", "f?o");

    assert_path_match("/foo", "foo");
    assert_path_match("/bar/foo", "foo");
    assert_path_not_match("/bar/foo/x", "foo");

    assert_is_c_identifier("a", 1);
    assert_is_c_identifier("z", 1);
    assert_is_c_identifier("A", 1);
    assert_is_c_identifier("Z", 1);
    assert_is_c_identifier("_", 1);
    assert_is_c_identifier("0", 0);
    assert_is_c_identifier("0x", 0);
    assert_is_c_identifier("foo", 1);
    assert_is_c_identifier("foo1", 1);
    assert_is_c_identifier("FOO1", 1);
    assert_is_c_identifier("FOO_1", 1);
    assert_is_c_identifier("FOO_1&", 0);
    assert_is_c_identifier("FOO_1&x", 0);
}