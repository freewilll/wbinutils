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

// For error reporting
static char *error_filename;
static int error_line;

// Set the filename used for error reporting
void set_error_filename(char *filename) {
    error_filename = filename;
}

// Set the line number used for error reporting
void set_error_line(int line) {
    error_line = line;
}

static void print_filename_and_linenumber(void) {
    int is_tty = isatty(2);
    if (is_tty) fprintf(stderr, LOCUS);

    if (error_line)
        fprintf(stderr, "%s:%d: ", error_filename, error_line);
    else
        fprintf(stderr, "%s: ", error_filename);

    if (is_tty) fprintf(stderr, RESET);
}

// Report an internal error and exit
void panic(char *format, ...) {
    va_list ap;
    va_start(ap, format);

    int is_tty = isatty(2);

    if (is_tty) fprintf(stderr, BRED);
    fprintf(stderr, "Internal error: ");
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    if (is_tty) fprintf(stderr, RESET);

    va_end(ap);
    exit(1);
}

static void verror(char *format, va_list ap) {
    int is_tty = isatty(2);
    if (is_tty) fprintf(stderr, BRED);
    fprintf(stderr, "error: ");
    if (is_tty) fprintf(stderr, RESET);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

// Report an error and exit
void error(char *format, ...) {
    va_list ap;
    va_start(ap, format);
    verror(format, ap);
}

// Report an error with filename and line number and exit
void error_in_file(char *format, ...) {
    va_list ap;
    va_start(ap, format);
    print_filename_and_linenumber();
    verror(format, ap);
}
