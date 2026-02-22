int f() {
    return 1;
}
__asm__(".symver f,f@V1");
