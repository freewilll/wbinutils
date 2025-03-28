#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// ANSI color codes
#define LOCUS "\e[0;01m" // Bold
#define BRED "\e[1;31m"  // Bright red
#define BMAG "\e[1;35m"  // Bright magenta
#define RESET "\e[0m"    // Reset

// Report an internal error and exit
void panic(char *format, ...) {
    va_list ap;
    va_start(ap, format);
    fprintf(stderr, "Internal error: ");
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

// Report an error and exit
void _error(char *format, va_list ap) {
    int is_tty = isatty(2);
    if (is_tty) fprintf(stderr, BRED);
    fprintf(stderr, "error: ");
    if (is_tty) fprintf(stderr, RESET);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

void error(char *format, ...) {
    va_list ap;
    va_start(ap, format);
    _error(format, ap);
}
