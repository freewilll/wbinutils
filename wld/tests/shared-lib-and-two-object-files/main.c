// Use a function and an integer in a library
// The library itself consists of two object files

extern int i;

int f1();

int main() {
    return f1() + i - 42;
}
