#ifndef _RO_ELF_H
#define _RO_ELF_H

#include <stdio.h>

#include "list.h"
#include "elf.h"
#include "strmap.h"

typedef struct section {
    ElfSectionHeader *elf_section_header; // Pointer to the ELF header
    int index;                            // Index in the ELF section headers table
    int offset;                           // Offset in output
} Section;

// In-memory input ELF file
typedef struct input_elf_file {
    const char *filename;               // Filename of input file
    FILE *file;                         // Open file handle
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
void load_section_into_buffer(ElfFile *elf_file, int section_index, void *dst);

#endif
