#ifndef _WLD_RW_ELF_H
#define _WLD_RW_ELF_H

#include <stdint.h>

#include "elf.h"
#include "list.h"
#include "strmap.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ALIGN_UP(offset, alignment) ((((offset) + alignment - 1) & ~(alignment - 1)))
#define ALIGN_DOWN(offset, alignment) (((offset) & ~(alignment - 1)))

typedef struct rw_section  {
    int index;                       // Index in the section header table
    const char *name;                // Name of the section
    int type;                        // Type of the section
    int flags;                       // Section attributes
    int align;                       // Contains the required alignment of the section. This field must be a power of two.
    int link;                        // Contains the section index of an associated section.
    int info;                        // Contains extra information about the section.
    char *data;                      // Contents of the section
    int allocated;                   // Amount of bytes allocated for data
    int size;                        // Size in bytes
    int offset;                      // Offset in the file
    int address;                     // Address in the executable
    long entsize;                    // Contains the size, in bytes, of each entry, for sections that contain fixed-size entries. Otherwise, this field contains zero.
    long symtab_index;               // Index in the symbol table for this section
    struct rw_section *rela_section; // Optional related relocation section
    int program_segment_index;       // Used by wld
    List *chunks;                    // Used by was
    int keep;                        // Include empty sections
    int layout_complete;             // This section has been allocated an offset and address
} RwSection;

typedef struct rw_elf  {
    const char *filename;                                   // Output filenamne
    int type;                                               // Object file type, one of ET_*
    List *sections_list;                                    // List of RwSections
    StrMap *sections_map;                                   // Map of the same RwSections as in the list
    RwSection *section_text;                                // Optionally pre-created sections ...
    RwSection *section_data;
    RwSection *section_bss;
    RwSection *section_rodata;
    RwSection *section_symtab;
    RwSection *section_strtab;
    RwSection *section_shstrtab;                            // Section header string table
    RwSection *section_got;                                 // Global Offset Table section
    int local_symbol_end;                                   // Index of last local symbol
    uint64_t entrypoint;                                    // Entry point of executable
    uint64_t tls_template_offset;                           // Offset in the file for Thread Local Storage (TLS) template
    uint64_t tls_template_tdata_size;                       // Size of the data part of the TLS template
    uint64_t tls_template_tbss_size;                        // Size of the bss part of the TLS template
    uint64_t tls_template_size;                             // Size of the TLS template
    uint64_t tls_template_address;                          // Virtual address of the TLS template
    uint64_t got_virt_address;                              // Virtual address of the GOT
    List *program_segments_list;                            // Used by wld
    int elf_program_segments_count;                         // ELF: Amount of program segment headers
    int elf_program_segments_header_size;                   // ELF: Size of the program segment headers
    ElfProgramSegmentHeader *elf_program_segment_headers;   // ELF: The encoded of the program segment headers
    int elf_section_headers_size;                           // ELF: The size of the section headers
    ElfSectionHeader *elf_section_headers;                  // ELF: The section headers
    int elf_program_segments_offset;                        // ELF: Offset in the ELF file of the program segments
    int elf_section_headers_offset;                         // ELF: Offset in the ELF file of the section headers
    int size;                                               // ELF: size of the output
    uint8_t *data;                                          // ELF: contents of the output
} RwElfFile;

// Sections
RwSection *get_rw_section(RwElfFile *elf_file, const char *name);
RwSection *add_rw_section(RwElfFile *rw_elf_file, const char *name, int type, int flags, int align);
int add_to_rw_section(RwSection *section, const void *src, int size);
int add_repeated_value_to_rw_section(RwSection *section, char value, int size);
int add_zeros_to_rw_section(RwSection *section, int size);
int add_elf_symbol(RwElfFile *output_elf_file, const char *name, long value, long size, int binding, int type, int visibility, int section_index);
void add_file_symbol(RwElfFile *output_elf_file, char *filename);
void add_elf_relocation(RwElfFile *output_elf_file, RwSection *section, int type, int symbol_index, long offset, long addend);

// ELF file
RwElfFile *new_rw_elf_file(const char *filename, int type);
void make_elf_headers(RwElfFile *output);
void make_section_indexes(RwElfFile *output_elf_file);
uint64_t headers_size(RwElfFile *elf_file);
void make_rw_section_header(RwElfFile *output_elf_file, ElfSectionHeader *sh, RwSection *section);
void make_rw_section_headers(RwElfFile *output_elf_file);
void make_program_segment_headers(RwElfFile *output);
void layout_rw_elf_sections(RwElfFile *output_elf_file);
void copy_rw_sections_to_elf(RwElfFile *output_elf_file);
void write_elf_file(RwElfFile *output_elf_file);

#endif
