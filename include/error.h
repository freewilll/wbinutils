#ifndef _ERROR_H
#define _ERROR_H

void set_error_filename(char *filename);
void set_error_line(int line);

void panic(char *format, ...);
void error(char *format, ...);
void error_in_file(char *format, ...);

#endif
