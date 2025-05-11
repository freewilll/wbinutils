#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

__thread int bss; // Zero default value that goes in .tbss

int main() {
    printf("Hello World! %d\n", bss);
}
