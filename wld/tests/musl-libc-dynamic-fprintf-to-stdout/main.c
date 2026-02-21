#include <stdio.h>

// The values are copied into the main during load time using a R_X86_64_COPY relocation
// This also happens when accessing stdout in musl.
extern char foo0;
extern int foo1;

int f();
int g();

int main() {
    // Test use of stdout from musl's lib
    fprintf(stdout, "Hello World!\n");

    if (foo0 != 1) return 10 + foo0;
    if (f() != 6) return 20 + f();
    if (g() != 6) return 30 + g();
    if (foo1 != 42) return 40 + foo1;
}
