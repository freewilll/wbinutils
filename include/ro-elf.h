#ifndef _RO_ELF_H
#define _RO_ELF_H

#include <stdio.h>

#include "list.h"
#include "elf.h"
#include "strmap.h"
#include "rw-elf.h"

typedef struct section {
    char *name;                           // Section name
    int index;                            // Index in the ELF section headers table
    uint64_t size;                        // Size in bytes of the section in the file image. May be 0.
    uint32_t type;                        // Identifies the type of this header.
    uint64_t flags;                       // Identifies the attributes of the section
    uint64_t src_offset;                  // Offset of the section in the file image
    uint32_t info;                        // Contains extra information about the section, from ELF
    uint64_t align;                       // Alignment
    int dst_offset;                       // Offset in output section
    RwSection *dst_section;               // Target Section (for WLD)
    void *data;                           // Potentially loaded section, or synthetic section
} Section;

// In-memory input ELF file
typedef struct input_elf_file {
    const char *filename;               // Filename of input file
    FILE *file;                         // Open file handle
    int file_offset;                    // Offset in file where the object begins, used for archives
    ElfHeader *elf_header;
    ElfSectionHeader *section_headers;  // All section headers
    char *section_header_strings;
    List *section_list;
    StrMap *section_map;
    char *strtab_strings;
    ElfSymbol *symbol_table;
    int symbol_count;
} ElfFile;

ElfFile *open_elf_file(const char *filename);
ElfFile *open_elf_file_in_archive(FILE *f, const char *filename, int offset);
void *load_section_uncached(ElfFile *elf_file, int section_index);
void *load_section(ElfFile *elf_file, Section *section);
void dump_symbols(ElfFile *elf_file);

#endif
