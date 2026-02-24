// Test building a library that calls a hidden symbol in a .o file that us also used when
// making an executable that calls the library.
// See the Makefile for a longer description.

int f();

int g() {
    return f();
}
