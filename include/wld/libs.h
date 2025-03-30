#ifndef _LIBS_H
#define LIBS_H

#include <unistd.h>

#include "list.h"

#define AR_MAGIC "!<arch>\n"
#define AR_MAGIC_LEN 8

typedef struct archive_file_object {
    char *filename;
    int offset;
} ArchiveFileObject;

typedef struct ar_header {
    char ar_name[16];    // File name
    char ar_date[12];    // File modification timestamp
    char ar_uid[6];      // Owner ID
    char ar_gid[6];      // Group ID
    char ar_mode[8];     // File mode (octal)
    char ar_size[10];    // Length of this member in bytes
    char ar_fmag[2];     // "`\n" (ARFMAG)
} ArHeader;

typedef struct archive_file {
    char *filename;     // Filename
    FILE *file;         // Open file handle
    List *objects;      // A list of ArchiveFileObject
} ArchiveFile;

extern const char *BUILTIN_LIBRARY_PATHS[];

char *search_for_library(List *library_paths, const char *input_filename);
ArchiveFile *open_archive_file(const char *filename);

#endif
