#pragma weak f
int f();

int g() {
    return f();
}
