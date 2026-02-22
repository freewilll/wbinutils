int f1(); // Binds to default symbol
int f2(); // In libtest2

int main() {
    if (f1() != 1) return 1;
    if (f2() != 1) return 1;
}
