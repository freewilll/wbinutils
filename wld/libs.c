#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "error.h"
#include "ro-elf.h"

#include "wld/libs.h"

const char *BUILTIN_LIBRARY_PATHS[] = {
    "/usr/local/lib/x86_64-linux-gnu",
    "/lib/x86_64-linux-gnu",
    "/usr/lib/x86_64-linux-gnu",
    "/usr/lib/x86_64-linux-gnu64",
    "/usr/local/lib64",
    "/lib64",
    "/usr/lib64",
    "/usr/local/lib",
    "/lib",
    "/usr/lib",
    "/usr/x86_64-linux-gnu/lib64",
    "/usr/x86_64-linux-gnu/lib",
};

// Try and find the library on the builtin library paths
char *search_for_library(List *library_paths, const char *filename) {
    char *path = malloc(strlen(filename) + 5);
    sprintf(path, "lib%s.a", filename);
    int path_len = strlen(path);

    int found = -1;
    char *test_path;

    // Search user paths
    for (int i = 0; i < library_paths->length; i++) {
        char *search_path = (char *) library_paths->elements[i];

        test_path = malloc(strlen(search_path) + path_len + 1);
        sprintf(test_path, "%s/%s", search_path, path);

        if (!access(test_path, F_OK)) {
            found = i;
            break;
        }
    }

    if (found == -1) {
        // Search system paths
        int system_library_count = sizeof(BUILTIN_LIBRARY_PATHS) / sizeof(BUILTIN_LIBRARY_PATHS[0]);
        for (int i = 0; i < system_library_count; i++) {
            test_path = malloc(strlen(BUILTIN_LIBRARY_PATHS[i]) + path_len + 1);
            sprintf(test_path, "%s/%s", BUILTIN_LIBRARY_PATHS[i], path);

            if (!access(test_path, F_OK)) {
                found = i;
                break;
            }
        }
    }

    if (found == -1) error("Cannot find library %s", filename);

    free(path);

    return test_path;
}

// Loop over all files in the archive,
// finding the filename and offset and storing them in objects.
// All entries have a header, which has a size in it.
void index_archive_file(ArchiveFile *ar_file) {
    ar_file->objects = new_list(32);

    // Any archive files with long filenames have a special object "//" which
    // contains the extended filename strings.
    // Filenames with the format "/123" can find the actual filename at offset 123.
    char *extended_filenames = NULL;
    char *extended_filenames_end = NULL;

    // Loop over all entries
    while (1) {
        ArHeader header;
        int read = fread(&header, 1, sizeof(ArHeader), ar_file->file);
        if (read != sizeof(ArHeader)) break;

        // Parse size
        char size_string[11];
        memcpy(size_string, header.ar_size, 10);
        size_string[10] = '\0';
        long size = strtol(size_string, NULL, 10);

        // Parse name
        char ar_name[17];
        char *name = NULL;
        memcpy(ar_name, header.ar_name, 16);
        ar_name[16] = '\0';
        char *last_slash = strrchr(ar_name, '/');
        if (!last_slash) error("Ar archive %s header is missing a slash", ar_file->filename);

        // Ar contents are 2-byte aligned
        int seek_alignment = size & 1;

        if (last_slash == ar_name) {
            // The entry starts with a leading slash

            if (ar_name[1] >= '0' && ar_name[1] <= '9') {
                // Extended filename
                long offset = strtol(&ar_name[1], NULL, 10);
                if (!extended_filenames) error("Ar archive %s is missing an extended filename file, used by an entry", ar_file->filename);
                name = &extended_filenames[offset];

                // Skip to first newline and then terminate the string at the preceding, character a '/'
                char *p = name;
                while (p < extended_filenames_end && *p != '\n') p++;
                *(p - 1) = 0;
            }
            else  {
                name = NULL;
            }
        }
        else if (last_slash == ar_name + 1) {
            // The entry is the special one "//", which means it contains extended filenames.

            extended_filenames = malloc(size);
            int read = fread(extended_filenames, 1, size, ar_file->file);
            if (read != size) error("Unable to read input file: %s", ar_file->filename);
            extended_filenames_end = extended_filenames+ size;
            size = 0; // Disable the contents seek further down
        }
        else {
            // Regular (non-extended) filename

            *last_slash = 0; // Null terminate the string
            name = ar_name;
        }

        if (name) {
            // Append to the objects list

            ArchiveFileObject *ar_file_object = malloc(sizeof(ArchiveFileObject));
            ar_file_object->filename = strdup(name);
            ar_file_object->offset = ftell(ar_file->file);
            append_to_list(ar_file->objects, ar_file_object);
        }

        // Seek to the next entry
        int seek_size = size + seek_alignment;
        if (fseek(ar_file->file, seek_size, SEEK_CUR) != 0) break;
    }
}

ArchiveFile *open_archive_file(const char *filename) {
    ArchiveFile *ar_file = malloc(sizeof(ArchiveFile));

    ar_file->filename = strdup(filename);

    ar_file->file = fopen(filename, "r");

    if (ar_file->file == 0) {
        perror(filename);
        exit(1);
    }

    // Check the magic string
    char magic[AR_MAGIC_LEN];
    if (fread(magic, 1, AR_MAGIC_LEN, ar_file->file) != AR_MAGIC_LEN)
        error("Unable to read input file: %s", filename);

    index_archive_file(ar_file);

    return ar_file;
}

void dump_archive_file_symbols(ArchiveFile* ar_file) {
    // Loop over all object files
    for (int i = 0; i < ar_file->objects->length; i++) {
        ArchiveFileObject *ar_file_object = (ArchiveFileObject *) ar_file->objects->elements[i];
        char *pseudo_filename = malloc(strlen(ar_file->filename) + strlen(ar_file_object->filename) + 2);
        sprintf(pseudo_filename, "%s/%s", ar_file->filename, ar_file_object->filename);
        ElfFile *elf_file = open_elf_file_in_archive(ar_file->file, pseudo_filename, ar_file_object->offset);
        dump_symbols(elf_file);
    }
}