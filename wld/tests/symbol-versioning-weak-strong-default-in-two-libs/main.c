int f(); // Binds to default weak symbol in libtest2

int main() {
    if (f() != 2) return 1;
}
