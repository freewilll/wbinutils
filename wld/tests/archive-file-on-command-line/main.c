#include <stdio.h>

extern int i;

int f();

int main(void){
    if (i != 42) return 1;
    if (f() != 42) return 2;

    return 0;
}
