#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "error.h"
#include "list.h"
#include "input-elf.h"
#include "strmap.h"

#include "wld/libs.h"
#include "wld/symbols.h"
#include "wld/utils.h"
#include "wld/wld.h"

// Try and find the library on the builtin library paths
char *search_for_library(List *library_paths, const char *name) {
    char *filename = malloc(strlen(name) + 6);
    sprintf(filename, "lib%s.a", name);
    char *path = find_file(library_paths, filename, "library");
    free(filename);
    return path;
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

// Check if an archive file starts with a GNU magic string
int is_gnu_linker_script_file(const char *filename) {
    FILE *file = fopen(filename, "r");

    int result = 0;

    char magic[GNU_LD_SCRIPT_MAGIC_LEN];
    if (fread(magic, 1, GNU_LD_SCRIPT_MAGIC_LEN, file) == GNU_LD_SCRIPT_MAGIC_LEN) {
        if (!memcmp(magic, GNU_LD_SCRIPT_MAGIC, GNU_LD_SCRIPT_MAGIC_LEN)) {
            result = 1;
        }
    }

    fclose(file);
    return result;
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
        error("Unable to read archive file: %s", filename);

    if (memcmp(magic, AR_MAGIC, AR_MAGIC_LEN))
        error("Not an archive file: %s", filename);

    index_archive_file(ar_file);

    return ar_file;
}

// Process an archive and return the amount of object files that were added to the link
int process_library_symbols(ArchiveFile *ar_file, List *input_elf_files) {
    StrMap *included_objects_map = new_strmap();
    List *included_objects_list = new_list(32);

    int objects_added;
    int total_objects_added = 0;

    // Repeat over all objects in the archive file until no objects have
    // been added. These multiple passes are needed, since an object might need a
    // symbol in a previously scanned object.

    do {
        objects_added = 0;

        // Loop over all object files
        for (int i = 0; i < ar_file->objects->length; i++) {
            ArchiveFileObject *obj = ar_file->objects->elements[i];

            // Don't process the same object twice
            if (strmap_get(included_objects_map, obj->filename)) continue;

            InputElfFile *elf_file = open_elf_file_in_archive(ar_file->file, obj->filename, obj->offset);
            if (DEBUG_SYMBOL_RESOLUTION) printf("Examining file %s in archive %s\n", elf_file->filename, ar_file->filename);
            int resolved_symbols = process_elf_file_symbols(elf_file, 1, 1);
            if (resolved_symbols) {
                // Use the object file
                if (!strmap_get(included_objects_map, obj->filename)) {
                    process_elf_file_symbols(elf_file, 1, 0);
                    strmap_put(included_objects_map, obj->filename, elf_file);
                    append_to_list(included_objects_list, elf_file);
                    objects_added++;
                    total_objects_added++;
                }
            }
        }
    } while (objects_added);

    for (int i = 0; i < included_objects_list->length; i++) {
        InputElfFile *elf_file = included_objects_list->elements[i];
        append_to_list(input_elf_files, elf_file);
        if (DEBUG_SYMBOL_RESOLUTION) printf("Including %s\n", elf_file->filename);
    }

    free_strmap(included_objects_map);
    free_list(included_objects_list);

    return total_objects_added;
}

void dump_archive_file_symbols(ArchiveFile* ar_file) {
    // Loop over all object files
    for (int i = 0; i < ar_file->objects->length; i++) {
        ArchiveFileObject *ar_file_object = (ArchiveFileObject *) ar_file->objects->elements[i];
        char *pseudo_filename = malloc(strlen(ar_file->filename) + strlen(ar_file_object->filename) + 2);
        sprintf(pseudo_filename, "%s/%s", ar_file->filename, ar_file_object->filename);
        InputElfFile *elf_file = open_elf_file_in_archive(ar_file->file, pseudo_filename, ar_file_object->offset);
        dump_symbols(elf_file);
    }
}
