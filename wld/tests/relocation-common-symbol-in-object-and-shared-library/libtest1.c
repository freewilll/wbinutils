int def1 = 1; // Goes in .data
int def2 = 2; // Goes in .data
int undef1;   // Common, goes in the .bss
int undef2;   // Common, goes in the .bss

int f(void) {
    return def1 == 1 && def2 == 2 && !undef1 && !undef2;
}
