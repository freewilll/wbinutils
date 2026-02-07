int f1_1() {
    return 1;
}

int f1_2() {
    return 2;
}

int f2_1() {
    return 1;
}

int f2_2() {
    return 2;
}

__asm__(".symver f1_1,f1@@V1");  // default
__asm__(".symver f1_2,f1@V2");   // non-default

__asm__(".symver f2_1,f2@V1");   // non-default
__asm__(".symver f2_2,f2@@V2");  // default
