// Test a R_X86_64_64 relocation in a shared library

int f();
char *foo();
char *bar();

int main() {
    if (f() != 42) return 1;

    char *s = foo();
    if (s[0] != 'f' && s[1] != 'o' && s[2] != 'o' && s[4] != 0) return 2;

    s = bar();
    if (s[0] != 'b' && s[1] != 'a' && s[2] != 'r' && s[3] != 0) return 3;
}
