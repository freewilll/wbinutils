#pragma weak f
int f() {
    return 2;
}
__asm__(".symver f,f@@V1");
