// Test the conversion of a R_X86_64_64 to a R_X86_64_RELATIVE in dynamic executables

// Three cases are tested here:
// - A relocation from a section symbol, here that's the string literal in the .bss section
// - A relocation from initialized data, in a regular .data section
// - A relocation from uninitialized  data, in the .bss section

#include <stdio.h>

char *foo = "Hello World!";
char **pfoo = &foo;

// Initialised
int x = 42;
long *px = &x;

// Uninitialised
int y;
long *py = &y;

// A relocation with a non-zero addend
struct s {
    int i, j;
};
struct s s;
int *psj = &s.j;

int main() {
    y = 43;
    s.j = 44;

    printf("%s\n", *pfoo);

    return !(*px == 42 && *py == 43 && *psj == 44);
}
