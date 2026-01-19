// Test a case of a function pointer to a function in a library
// A R_X86_64_64 relocation must be present in .rela.dyn

#include <stdio.h>
#include <stdlib.h>

static void (*pexit)(int) = exit;

int main(void) {
    pexit(0);
}
