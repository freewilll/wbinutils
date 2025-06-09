#ifndef _UTILS_H
#define _UTILS_H

#include "list.h"

typedef enum file_type {
    FT_UNKNOWN,
    FT_ARCHIVE,
    FT_SHARED_LIBRARY,
    FT_LINKER_SCRIPT,
} FileType;

char *find_file(List *library_paths, const char *filename, const char *what);
char *must_find_file(List *library_paths, const char *filename, const char *what);
FileType identify_library_file(const char *filename);
int match_pattern(const char *string, const char *pattern);
int match_path_pattern(const char *filename, const char *pattern);
int is_c_identifier(const char *name);

#endif
