#ifndef _UTILS_H
#define _UTILS_H

#include "list.h"

char *find_file(List *library_paths, const char *path, const char *what);
int match_pattern(char *string, char *pattern);

#endif
