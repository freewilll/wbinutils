#ifndef _UTILS_H
#define _UTILS_H

void panic(char *format, ...);
void error(char *format, ...);

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ALIGN_UP(offset, alignment) ((((offset) + alignment - 1) & ~(alignment - 1)))

#endif
