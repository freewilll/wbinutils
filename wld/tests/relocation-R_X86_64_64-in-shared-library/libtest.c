int d = 42;
int *pd = &d; // Creates a R_X86_64_64 relocation

int f() {
    return *pd;
}
