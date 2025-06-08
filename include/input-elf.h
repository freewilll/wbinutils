#ifndef _RO_ELF_H
#define _RO_ELF_H

#include <stdio.h>

#include "list.h"
#include "elf.h"
#include "strmap.h"
#include "output-elf.h"

typedef struct input_section {
    char *name;                           // Section name
    int index;                            // Index in the ELF section headers table
    int allocated;                        // Amount of bytes allocated for data, when dynamically adding to data
    void *data;                           // Potentially loaded section, or synthetic section
    uint64_t size;                        // Size in bytes of the section in the file image. May be 0.
    uint32_t type;                        // Identifies the type of this header.
    uint64_t flags;                       // Identifies the attributes of the section
    uint64_t src_offset;                  // Offset of the section in the file image
    uint32_t info;                        // Contains extra information about the section, from ELF
    uint64_t align;                       // Alignment
    int dst_offset;                       // Offset in output section
    OutputSection *output_section;        // Target Section (for WLD)
} InputSection;

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
} InputElfFile;

InputElfFile *open_elf_file(const char *filename);
InputElfFile *open_elf_file_in_archive(FILE *f, const char *filename, int offset);
void *load_section_uncached(InputElfFile *elf_file, int section_index);
void *load_section(InputElfFile *elf_file, InputSection *section);
void *allocate_in_section(InputSection *section, int size);
int add_to_input_section(InputSection *section, const void *src, int size);
void dump_symbols(InputElfFile *elf_file);

#endif
