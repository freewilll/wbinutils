#ifndef _WLD_H
#define _WLD_H

#include "list.h"
#include "input-elf.h"
#include "output-elf.h"

// Flags that determine the output type
#define OUTPUT_TYPE_FLAG_STATIC       1
#define OUTPUT_TYPE_FLAG_SHARED       2
#define OUTPUT_TYPE_FLAG_EXECUTABLE   4

#define DEFAULT_ENTRYPOINT_SYMBOL_NAME  "_start"

#define MAXPAGESIZE    0x1000
#define COMMONPAGESIZE 0x1000

#define BASE_DYNAMIC_SECTION_ENTRY_COUNT 7

#define GOT_SECTION_NAME        ".got"
#define GOT_PLT_SECTION_NAME    ".got.plt"
#define GOT_IPLT_SECTION_NAME   ".got.iplt"
#define PLT_SECTION_NAME        ".plt"
#define IPLT_SECTION_NAME       ".iplt"
#define RELA_PLT_SECTION_NAME   ".rela.plt"
#define RELA_IPLT_SECTION_NAME  ".rela.iplt"
#define RELA_DYN_SECTION_NAME   ".rela.dyn"
#define DYNAMIC_SECTION_NAME    ".dynamic"
#define DYNSTR_SECTION_NAME     ".dynstr"
#define DYNSYM_SECTION_NAME     ".dynsym"
#define HASH_SECTION_NAME       ".hash"
#define INTERP_SECTION_NAME     ".interp"
#define DATA_COPY_SECTION_NAME  ".data.copy"
#define VERSYM_SECTION_NAME     ".gnu.version"
#define VERNEED_SECTION_NAME    ".gnu.version_r"
#define VERDEF_SECTION_NAME     ".gnu.version_d"
#define INIT_SECTION_NAME       ".init"
#define FINI_SECTION_NAME       ".fini"
#define INIT_ARRAY_SECTION_NAME ".init_array"
#define FINI_ARRAY_SECTION_NAME ".fini_array"

#define DEBUG_SYMBOL_RESOLUTION     0
#define DEBUG_SYMBOL_VERSIONS       0
#define DEBUG_RELOCATIONS           0
#define DEBUG_RELOCATION_RELAXATION 0
#define DEBUG_LAYOUT                0

// Section types that are included when not referenced in the linker script
#define ORPHANED_SECTION_TYPE(type) \
    ( \
        ((type) == SHT_PROGBITS) || \
        ((type) == SHT_NOBITS) \
    )

typedef struct input_file {
    char *filename;
    int is_library_name;       // Passed on the command line with -l
} InputFile;

extern const char *DYNAMIC_SECTION_TYPE_NAMES[];

const char *dynamic_section_name(uint64_t tag);
const char *section_type_name(uint64_t type);
OutputElfFile *init_output_elf_file(const char *output_filename, int output_type);
InputSection *get_extra_section(OutputElfFile *output_elf_file, char *name);
InputSection *create_extra_section(OutputElfFile *output_elf_file, char *name, uint32_t type, uint64_t flags, uint64_t align);
InputSection *get_or_create_extra_section(OutputElfFile *output_elf_file, char *name, uint32_t type, uint64_t flags, uint64_t align);
void dump_dynamic_section(OutputElfFile *output_elf_file);
void dump_relocations(OutputSection* section);
void dump_sections(OutputElfFile *output_elf_file);
void dump_program_segments(OutputElfFile *output_elf_file);
OutputSection *get_or_create_create_bss_section(OutputElfFile *output);
OutputElfFile *run(List *library_paths, List *linker_scripts, List *input_files, const char *output_filename, int output_type, char *dynamic_linker, char *soname, List *rpaths);

#endif
