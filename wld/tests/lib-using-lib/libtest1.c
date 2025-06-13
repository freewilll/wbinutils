extern int i;
extern int j;

int f2();
int f3();

int f1() {
    return i + j + f2() + f3() * 2;
}
