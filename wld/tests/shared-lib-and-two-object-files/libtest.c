// A library that consists of two object files
// f2 and j are both relocations

int i = 11;

// In libfuncs.c
extern int j;
int f2();

int f1() {
    return 12 + j +  f2();
}
