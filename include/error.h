#ifndef _ERROR_H
#define _ERROR_H

#include <stdarg.h>

void set_error_filename(char *filename);
void set_error_line(int line);

void panic(char *format, ...);
void error(char *format, ...);
void error_in_file(char *format, ...);
void verror_in_file(char *format, va_list ap);

#endif
