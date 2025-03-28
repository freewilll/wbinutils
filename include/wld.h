#ifndef _WLD_H
#define _WLD_H

#include "list.h"

#define EXECUTABLE_VIRTUAL_ADDRESS 0x400000

void run(List *input_filenames, const char *output_filename);

#endif
