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
    uint32_t link;                        // Associated section
    uint64_t align;                       // Alignment
    uint64_t dst_offset;                  // Offset in output section
    OutputSection *output_section;        // Target Section (for wld)
} InputSection;

// In-memory input ELF file
typedef struct input_elf_file {
    const char *filename;               // Filename of input file
    FILE *file;                         // Open file handle
    int file_offset;                    // Offset in file where the object begins, used for archives
    ElfHeader *elf_header;
    ElfSectionHeader *section_headers;  // All section headers
    int type;                           // Set to elf_header->type
    char *section_header_strings;
    List *section_list;
    StrMap *section_map;
    char *symbol_table_strings;
    ElfSymbol *symbol_table;
    int symbol_count;
    uint16_t *symbol_table_version_indexes; // Indexed by symbol index, the version index of the symbol
    int *non_default_versioned_symbols;     // Boolean list, indexed by version index
    List *symbol_version_names;             // Version names, indexed by version index
    int *global_version_indexes;            // A table to lookup a global version index from the ELF file version index
} InputElfFile;

InputSection *get_input_section(InputElfFile *elf_file, char *name);
InputElfFile *open_elf_file(const char *filename);
InputElfFile *open_elf_file_in_archive(FILE *f, const char *filename, int offset);
int file_is_shared_library_file(const char *filename);
void *load_section_uncached(InputElfFile *elf_file, int section_index);
void *load_section(InputElfFile *elf_file, InputSection *section);
void *allocate_in_section(InputSection *section, int size);
int add_to_input_section(InputSection *section, const void *src, int size);
void dump_symbols(InputElfFile *elf_file);

#endif
