int d = 42;
int *pd = &d; // Creates a R_X86_64_64 relocation to a symbol

char *s1 = "foo"; // Creates a R_X86_64_64 relocation to the .rodata section with offset 0
char *s2 = "bar"; // Creates a R_X86_64_64 relocation to the .rodata section with offset 4

int f() {
    return *pd;
}

char *foo() {
    return s1;
}

char *bar() {
    return s2;
}
