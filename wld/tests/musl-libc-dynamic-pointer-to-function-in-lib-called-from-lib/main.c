// Test a case of a function pointer to a function in a library
// A R_X86_64_64 relocation must be present in .rela.dyn
// This tests the case of a main calling a library calling a library

#include <stdio.h>
#include <stdlib.h>

void f(void);

int main(void) {
    f();
}
