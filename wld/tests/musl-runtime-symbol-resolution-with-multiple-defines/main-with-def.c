#include <stdio.h>

int e = 1;

extern int f();
extern int g();

int main() {
    // Ensure e, f() and g() return the same result
    if (e != f()) return 1;
    if (e != g()) return 1;

    return e;
}
