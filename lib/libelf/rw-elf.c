#include "elf.h"
#include "list.h"
#include "strmap.h"
#include "rw-elf.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// Get a RW section. May return NULL if not existent
RwSection *get_rw_section(RwElfFile *elf_file, const char *name) {
    return strmap_get(elf_file->sections_map, name);
}

// Add a read/write section to an elf file
RwSection *add_rw_section(RwElfFile *rw_elf_file, const char *name, int type, int flags, int align) {
    RwSection *section = calloc(1, sizeof(RwSection));
    section->name = strdup(name);
    section->type = type;
    section->flags = flags;
    section->align = align;

    append_to_list(rw_elf_file->sections_list, section);
    strmap_put(rw_elf_file->sections_map, name, section);

    return section;
}

// Allocate space at the end of a section and return a pointer to it.
// Dynamically allocate space size as needed.
static void *allocate_in_section(RwSection *section, int size) {
    int new_section_size = section->size + size;
    if (new_section_size > section->allocated) {
        if (!section->allocated) section->allocated = 1;
        while (new_section_size > section->allocated) section->allocated *= 2;
        section->data = realloc(section->data, section->allocated);
    }

    void *result = section->data + section->size;
    section->size = new_section_size;

    return result;
}

// Copy src to the end of a section and return the offset
int add_to_rw_section(RwSection *section, void *src, int size) {
    char *data = allocate_in_section(section, size);
    memcpy(data, src, size);
    return data - section->data;
}

// Create a new RwElfFile object and set some defaults
RwElfFile *new_rw_elf_file(const char *filename, uint64_t executable_virt_address) {
    RwElfFile *result = malloc(sizeof(RwElfFile));

    result->filename = strdup(filename);
    result->sections_list = new_list(32);
    result->sections_map = new_strmap();
    result->executable_virt_address = executable_virt_address;

    return result;
}
