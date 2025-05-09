#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

__thread int foo = 42;

int main() {
    printf("Hello World! %d\n", foo);
}
