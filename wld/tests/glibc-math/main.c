#include <stdio.h>
#include <math.h>

int main(void) {
    double x = M_PI / 4.0;
    double xsin = sin(x); // 0.707

    return !(xsin > 0.7 && xsin < 0.8);
}