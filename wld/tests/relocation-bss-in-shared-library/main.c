// Test a dynamic executable that uses .bss symbols from a shared library

int i; // A common symbol
int j; // A common symbol
int k; // A common symbol
int l; // A common symbol

int main() {
    if (i != 0) return 1;
    if (j != 0) return 1;
    if (k != 0) return 1;
    if (l != 0) return 1;

    // Ensure the symbols don't overlap
    if (&i == &j) return 2;
    if (&i == &k) return 2;
    if (&i == &l) return 2;
}
