#ifndef _UTILS_H
#define _UTILS_H

#include "list.h"

char *find_file(List *library_paths, const char *path, const char *what);
int match_pattern(const char *string, const char *pattern);
int match_path_pattern(const char *filename, const char *pattern);

#endif
