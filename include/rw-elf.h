#ifndef _WLD_RW_ELF_H
#define _WLD_RW_ELF_H

#include <stdint.h>

#include "elf.h"
#include "list.h"
#include "strmap.h"

typedef struct rw_section  {
    int index;                    // Index in the section header table
    const char *name;             // Name of the section
    int offset;                   // Offset in the file
    int size;                     // Size in bytes
    int type;                     // Type of the section
    int flags;                    // Section attributes
    int align;                    // Contains the required alignment of the section. This field must be a power of two.
    char *data;                   // Contents of the section
    int allocated;                // Amount of bytes allocated for data
} RwSection;

typedef struct rw_elf  {
    const char *filename;                                   // Output filenamne
    List *sections_list;                                    // List of RwSections
    StrMap *sections_map;                                   // Map of the same RwSections as in the list
    RwSection *shstrtab;                                    // Section header string table
    uint64_t executable_virt_address;                       // Virtual address of executable
    int elf_program_segments_count;                         // ELF: Amount of program segment headers
    int elf_program_segments_header_size;                   // ELF: Size of the program segment headers
    ElfProgramSegmentHeader *elf_program_segment_headers;   // ELF: The encoded of the program segment headers
    int elf_section_headers_size;                           // ELF: The size of the section headers
    ElfSectionHeader *elf_section_headers;                  // ELF: The section headers
    int elf_program_segments_offset;                        // ELF: Offset in the ELF file of the program segments
    int elf_section_headers_offset;                         // ELF: Offset in the ELF file of the section headers
} RwElfFile;

RwSection *get_rw_section(RwElfFile *elf_file, const char *name);
RwSection *add_rw_section(RwElfFile *rw_elf_file, const char *name, int type, int flags, int align);
int add_to_rw_section(RwSection *section, void *src, int size);
RwElfFile *new_rw_elf_file(const char *filename, uint64_t executable_virt_address);

#endif