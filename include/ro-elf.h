#ifndef _RO_ELF_H
#define _RO_ELF_H

#include "list.h"
#include "elf.h"
#include "strmap.h"

typedef struct input_section {
    ElfSectionHeader *elf_section_header; // Pointer to the ELF header
    int offset;                           // Offset in output
} InputSection;

// In-memory input ELF file
typedef struct input_file {
    const char *filename;
    int size;
    char *data;
    ElfHeader *elf_header;
    const char *section_header_strings;
    List *section_list;
    StrMap *section_map;
    char *strtab_strings;
    ElfSymbol *symbol_table;
    int symbol_count;
} InputElfFile;

InputElfFile *read_elf_file(const char *filename);

#endif
