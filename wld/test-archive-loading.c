#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "error.h"
#include "wld/libs.h"

void assert_int(int expected, int actual, const char *message) {
    if (expected != actual) {
        printf("%s: expected %d, got %d\n", message, expected, actual);
        exit(1);
    }
}

void assert_string(const char *expected, const char *actual, const char *message) {
    if (strcmp(expected, actual)) {
        printf("%s: expected %s, got %s\n", message, expected, actual);
        exit(1);
    }
}

int main() {
    // Test libtest-archive-loading.a which only has one object in it
    ArchiveFile *file = open_archive_file("libtest-archive-loading.a");
    assert_int(1, file->objects->length, "The file has one object");
    ArchiveFile *af = file->objects->elements[0];
    assert_string("test-archive-loading.o", af->filename, "The file has test-archive-loading.o");

    // Test libtest-archive-loading-long.a which has two objects in it, one with a long filename
    file = open_archive_file("libtest-archive-loading-long.a");
    assert_int(2, file->objects->length, "The file has one object");
    af = file->objects->elements[0];
    assert_string("test-archive-loading.o", af->filename, "The file has test-archive-loading.o");
    af = file->objects->elements[1];
    assert_string("a-very-very-very-long-object-filename.o", af->filename, "The file has a-very-very-very-long-object-filename.o");
}
