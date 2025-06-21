#ifndef _SYMBOLS_H
#define _SYMBOLS_H

#include "input-elf.h"
#include "output-elf.h"
#include "map-ordered.h"
#include "strmap.h"
#include "strmap-ordered.h"

#include "wld/script.h"

#define GLOBAL_OFFSET_TABLE_SYMBOL_NAME "_GLOBAL_OFFSET_TABLE_"

// Symbol name & version
typedef struct symbol_nv {
    const char *name;
    uint16_t version_index;
} SymbolNV;

typedef struct symbol {
    char *name;                     // Name
    int version;                    // Version
    int binding;                    // Binding, e.g. local or global
    int type;                       // Type, e.g. function or object
    int other;                      // Visibility
    uint64_t size;
    int is_abs;                     // The src value is an absolute address
    int is_common;                  // The symbol is a common symbol. input_section is null.
    int visibility;                 // Used by the linker
    int src_is_library;             // 1 if the symbol was found in a library, otherwise it was found in an object file
    int src_is_shared_library;      // 1 is the symbol is a shared library. src_is_library is also 1 in this case.
    int needs_got;                  // Set if the symbol needs an entry in the Global Offset Table (GOT)
    int needs_got_plt;              // Set if the symbol needs an entry in the .got.plt table
    int needs_got_iplt;             // Set if the symbol needs an entry in the .got.iplt table, for ifuncs
    uint64_t got_offset;            // Offset in the .got section, if present
    uint64_t got_plt_offset;        // Offset in the .got.plt section, if present
    uint64_t got_iplt_offset;       // Offset in the .got.iplt section, if present
    uint64_t plt_offset;            // Offset in the .plt section, if present
    uint64_t iplt_offset;           // Offset in the .iplt section, if present
    uint64_t rela_iplt_offset;      // Offset in the .rela.iplt section, if present
    InputElfFile *src_elf_file;     // File symbol is defined in. NULL if undefined.
    InputSection *input_section;    // Section the symbol is defined in. NULL if the symbol is undefined or common.
    OutputSection *output_section;
    uint64_t src_value;             // Value in the original ELF section
    uint64_t dst_value;             // Value in the final ELF section
    int dst_index;                  // Index in the final ELF symbol table
    int dst_dynsym_index;           // Index in the final ELF dynsyn table (for libraries)
} Symbol;

typedef struct symbol_table {
    MapOrdered *defined_symbols;
    MapOrdered *undefined_symbols;
} SymbolTable;

extern SymbolTable *global_symbol_table;
extern StrMap *local_symbol_tables;

extern char *last_error_message;

SymbolTable *new_symbol_table(void);
void init_symbols(OutputElfFile *output_elf_file);
Symbol *get_defined_symbol(SymbolTable *st, const char *name, int version_index);
Symbol *get_global_defined_symbol(const char *name, int version_index);
Symbol *must_get_defined_symbol(SymbolTable *st, const char *name, int version_index);
Symbol *must_get_global_defined_symbol(const char *name, int version_index);
SymbolTable *get_local_symbol_table(InputElfFile *elf_file);
Symbol *lookup_symbol(InputElfFile *elf_file, char *name, int version_index);
Symbol *get_undefined_symbol(const char *name, int version_index);
int is_undefined_symbol(const char *name, int version_index);
Symbol *get_or_add_linker_script_symbol(CommandAssignment *assignment);
int process_elf_file_symbols(InputElfFile *elf_file, int is_library, int is_shared_library, int read_only);
void finalize_symbols(OutputElfFile *output_elf_file);
void dump_output_symbols(OutputElfFile *output_elf_file);
void debug_print_symbol(Symbol *symbol);
void debug_summarize_symbols(void);
int common_symbols_are_present(void);
void layout_common_symbols_in_bss_section(OutputSection *bss_section);
void make_symbol_values_from_symbol_table(OutputElfFile *output_elf_file, SymbolTable *symbol_table);
int add_dynstr_string(OutputElfFile *output_elf_file, const char *name);
void make_elf_dyn_symbols(OutputElfFile *output_elf_file);
void make_elf_symbols(OutputElfFile *output_elf_file);
void update_elf_symbols(OutputElfFile *output_elf_file);
void create_got_plt_and_rela_sections(OutputElfFile *output_elf_file);
void update_got_values(OutputElfFile *output_elf_file);
void update_dynamic_relocatable_values(OutputElfFile *output_elf_file);
void process_ifuncs_from_symbol_table(OutputElfFile *output_elf_file, SymbolTable *symbol_table);
void allocate_extra_sections(OutputElfFile *output_elf_file);;
void update_iplt(OutputElfFile *output_elf_file);
void make_symbol_hashes(OutputElfFile *output_elf_file);
void create_dyn_rela_section(OutputElfFile *output_elf_file);
void update_dyn_rela_section(OutputElfFile *output_elf_file);

#endif
