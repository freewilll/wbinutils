#include <stdio.h>

// f(), declared here, preempts the f() declared in the shared library.
int f() {
    return 1;
}

int g();

int main(void) {
    if (f() != 1) return 1;
    if (g() != 1) return 2;
}
