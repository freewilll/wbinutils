#include <stdio.h>

int def1;           // Req!uires a COPY relocation
extern int def2;    // Requires a COPY relocation
int undef1;         // Doesn't require a COPY relocation, since it goes into the executable's .bss
extern int undef2;  // Requires a COPY relocation, since it's in the .bss in libtest2

int f(void);
int g(void);
int h(void);

int main(void) {
    if (def1 != 1) return 1;
    if (def2 != 2) return 2;
    if (!f()) return 3;
    if (!g()) return 4;
    if (!h()) return 5;
    if (undef1 != 0) return 6;
    if (undef2 != 0) return 7;
}
