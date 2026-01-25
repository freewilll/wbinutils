// Test a R_X86_64_64 relocation in a shared library

extern int f();

int main() {
    return f() - 42;
}