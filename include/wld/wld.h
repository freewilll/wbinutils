#ifndef _WLD_H
#define _WLD_H

#include "list.h"

#define EXECUTABLE_VIRTUAL_ADDRESS 0x400000

typedef struct input_file {
    char *filename;
    int is_library;       // On the command line with -l
} InputFile;

void run(List *library_paths, List *input_files, const char *output_filename);

#endif
