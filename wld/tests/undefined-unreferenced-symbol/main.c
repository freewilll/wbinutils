#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main() {
    __asm (".globl _undefined_and_not_used");
}
