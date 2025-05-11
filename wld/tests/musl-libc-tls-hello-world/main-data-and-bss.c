#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

__thread int data = 42;
__thread int bss; // Zero default value that goes in .tbss

int main() {
    printf("Hello World! %d\n", data + bss);
}
