#ifndef _WLD_H
#define _WLD_H

#include "list.h"
#include "ro-elf.h"
#include "rw-elf.h"

#define OUTPUT_TYPE_STATIC   1
#define OUTPUT_TYPE_SHARED   2

#define DEFAULT_ENTRYPOINT_SYMBOL_NAME  "_start"

#define MAXPAGESIZE    0x1000
#define COMMONPAGESIZE 0x1000

#define DYNAMIC_SECTION_ENTRY_COUNT 5

#define GOT_SECTION_NAME        ".got"
#define GOT_PLT_SECTION_NAME    ".got.plt"
#define IPLT_SECTION_NAME       ".iplt"
#define RELA_IPLT_SECTION_NAME  ".rela.iplt"
#define DYNAMIC_SECTION_NAME    ".dynamic"
#define DYNSTR_SECTION_NAME     ".dynstr"
#define DYNSYM_SECTION_NAME     ".dynsym"

#define DEBUG_SYMBOL_RESOLUTION 0
#define DEBUG_RELOCATIONS 0
#define DEBUG_LAYOUT 0

// Section types that are included when not referenced in the linker script
#define ORPHANED_SECTION_TYPE(type) \
    ( \
        ((type) == SHT_PROGBITS) || \
        ((type) == SHT_NOBITS) \
    )

typedef struct input_file {
    char *filename;
    int is_library;       // On the command line with -l
} InputFile;

RwElfFile *init_output_elf_file(const char *output_filename, int output_type);
Section *get_extra_section(RwElfFile *output_elf_file, char *name);
Section *create_extra_section(RwElfFile *output_elf_file, char *name, uint32_t type, uint64_t flags, uint64_t align);
void dump_sections(RwElfFile *output_elf_file);
void dump_program_segments(RwElfFile *output_elf_file);
RwElfFile *run(List *library_paths, List *linker_scripts, List *input_files, const char *output_filename, int output_type);

#endif
