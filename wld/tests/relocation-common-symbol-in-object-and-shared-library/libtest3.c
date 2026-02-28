int def1;
int def2;
int undef1;
int undef2;

int h(void) {
    return def1 == 1 && def2 == 2 && !undef1 && !undef2;
}