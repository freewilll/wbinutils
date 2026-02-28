extern int def1;
extern int def2;
extern int undef1;
extern int undef2;

int g(void) {
    return def1 == 1 && def2 == 2 && !undef1 && !undef2;
}