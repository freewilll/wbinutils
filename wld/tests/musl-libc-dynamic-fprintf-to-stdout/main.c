#include <stdio.h>

// The value of i is copied into the main during load time using a R_X86_64_COPY relocation
// This also happens when accessing stdout in musl.
extern int object_in_shared_library;

int main() {
    // Test use of stdout from musl's lib
    fprintf(stdout, "Hello World!\n");

    // Test reading a copy of an object from the shared library.
    return object_in_shared_library - 42;
}
