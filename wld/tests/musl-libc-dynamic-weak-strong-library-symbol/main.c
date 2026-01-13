#include <stdio.h>

int i = 1; // A strong symbol that overrides the weak i in the library

int main() {
    return i - 1;
}
