#ifndef _WLD_RW_ELF_H
#define _WLD_RW_ELF_H

#include <stdint.h>

#include "elf.h"
#include "list.h"
#include "strmap.h"
#include "strmap-ordered.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ALIGN_UP(offset, alignment) ((((offset) + alignment - 1) & ~(alignment - 1)))
#define ALIGN_DOWN(offset, alignment) (((offset) & ~(alignment - 1)))

typedef struct output_section  {
    int index;                           // Index in the section header table
    const char *name;                    // Name of the section
    int type;                            // Type of the section
    int flags;                           // Section attributes
    int align;                           // Contains the required alignment of the section. This field must be a power of two.
    int link;                            // Contains the section index of an associated section.
    int info;                            // Contains extra information about the section.
    char *data;                          // Contents of the section
    int allocated;                       // Amount of bytes allocated for data
    uint64_t size;                       // Size in bytes
    uint64_t offset;                     // Offset in the file
    uint64_t address;                    // Address in the executable
    uint64_t entsize;                    // Contains the size, in bytes, of each entry, for sections that contain fixed-size entries. Otherwise, this field contains zero.
    uint64_t symtab_index;               // Index in the symbol table for this section
    struct output_section *rela_section; // Optional related relocation section
    int is_orphan;                       // Use by wld
    List *chunks;                        // Used by was
    int keep;                            // Include empty sections
    int layout_complete;                 // This section has been allocated an offset and address
    List *command_assignments;           // Used by wld, list of OutputSectionAssignment
} OutputSection;

typedef struct output_elf  {
    const char *filename;                                   // Output filename
    int type;                                               // Object file type, one of ET_*
    int is_executable;                                      // Object file type, one of ET_*
    List *sections_list;                                    // List of OutputSections
    StrMap *sections_map;                                   // Map of the same OutputSections as in the list
    OutputSection *section_text;                            // Optionally pre-created sections ...
    OutputSection *section_data;
    OutputSection *section_bss;
    OutputSection *section_rodata;
    OutputSection *section_symtab;
    OutputSection *section_strtab;
    OutputSection *section_shstrtab;                        // Section header string table
    struct input_section *section_dynsym;                   // Used by wld
    struct input_section *section_dynstr;                   // Used by wld
    struct input_section *section_hash;                     // Used by wld
    struct input_section *section_rela_dyn;                 // Used by wld
    struct input_section *section_rela_plt;                 // Used by wld
    struct input_section *section_got_plt;                  // Used by wld
    int local_symbol_end;                                   // Index of last local symbol
    uint64_t entrypoint;                                    // Entry point of executable
    uint64_t tls_template_offset;                           // Offset in the file for Thread Local Storage (TLS) template
    uint64_t tls_template_tdata_size;                       // Size of the data part of the TLS template
    uint64_t tls_template_tbss_size;                        // Size of the bss part of the TLS template
    uint64_t tls_template_size;                             // Size of the TLS template
    uint64_t tls_template_address;                          // Virtual address of the TLS template
    uint64_t got_virt_address;                              // Virtual address of the GOT
    uint64_t got_iplt_virt_address;                         // Virtual address of .got.iplt
    uint64_t plt_offset;                                    // Offset of .plt
    uint64_t iplt_virt_address;                             // Virtual address of the .iplt section
    List *program_segments_list;                            // Used by wld
    StrMapOrdered *extra_sections;                          // Used by wld
    List *ifunc_symbols;                                    // Used by wld
    List *linker_script;                                    // Used by wld
    StrMap *global_symbols_in_use;                          // Used by wld
    int dynsym_symbol_count;                                // Used by wld
    int rela_dyn_entry_count;                               // Used by wld
    int got_plt_entries_count;                              // Used by wld
    List *shared_libraries;                                 // Used by wld
    List *extra_rela_dyn_symbols;                           // Used by wld
    int elf_program_segments_count;                         // ELF: Amount of program segment headers
    uint64_t elf_program_segments_header_size;              // ELF: Size of the program segment headers
    ElfProgramSegmentHeader *elf_program_segment_headers;   // ELF: The encoded of the program segment headers
    uint64_t elf_section_headers_size;                      // ELF: The size of the section headers
    ElfSectionHeader *elf_section_headers;                  // ELF: The section headers
    uint64_t elf_program_segments_offset;                   // ELF: Offset in the ELF file of the program segments
    uint64_t elf_section_headers_offset;                    // ELF: Offset in the ELF file of the section headers
    uint64_t size;                                          // ELF: size of the output
    uint8_t *data;                                          // ELF: contents of the output
} OutputElfFile;

// Sections
OutputSection *get_output_section(OutputElfFile *elf_file, const char *name);
OutputSection *add_output_section(OutputElfFile *output_elf_file, const char *name, int type, int flags, int align);
void move_output_section(OutputElfFile *output_elf_file, OutputSection *output_section, int position);
int add_to_output_section(OutputSection *section, const void *src, int size);
int add_repeated_value_to_output_section(OutputSection *section, char value, int size);
int add_zeros_to_output_section(OutputSection *section, int size);
int add_elf_symbol(OutputElfFile *output_elf_file, const char *name, long value, long size, int binding, int type, int visibility, int section_index);
void add_file_symbol(OutputElfFile *output_elf_file, char *filename);
void add_elf_relocation(OutputElfFile *output_elf_file, OutputSection *section, int type, int symbol_index, long offset, long addend);

// ELF file
OutputElfFile *new_output_elf_file(const char *filename, int type);
void make_elf_headers(OutputElfFile *output);
void make_section_indexes(OutputElfFile *output_elf_file);
uint64_t headers_size(OutputElfFile *elf_file);
void make_output_section_header(OutputElfFile *output_elf_file, ElfSectionHeader *sh, OutputSection *section);
void make_output_section_headers(OutputElfFile *output_elf_file);
void make_program_segment_headers(OutputElfFile *output);
void layout_output_elf_sections(OutputElfFile *output_elf_file);
void copy_output_sections_to_elf(OutputElfFile *output_elf_file);
void write_elf_file(OutputElfFile *output_elf_file);

#endif
