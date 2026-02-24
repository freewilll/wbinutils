#ifndef _SYMBOLS_H
#define _SYMBOLS_H

#include "input-elf.h"
#include "output-elf.h"
#include "map-ordered.h"
#include "strmap.h"
#include "strmap-ordered.h"

#include "wld/script.h"

#define GLOBAL_OFFSET_TABLE_SYMBOL_NAME "_GLOBAL_OFFSET_TABLE_"
#define DYNAMIC_SYMBOL_NAME "_DYNAMIC"

// Symbol name & version
typedef struct symbol_nv {
    const char *name;
    uint16_t version_index;
    char *full_name;            // Name with version, e,g foo, foo@V1, foo@@V2
    int is_default;             // Set if this symbol is the default for a name
    int is_proxy_for_default;   // Set if this SymbolNV is a duplicate that points to a default symbol
} SymbolNV;

// Some constants to indicate the origin of a symbol
#define SRC_INTERNAL       1 // Created by the linker
#define SRC_OBJECT         2 // Seen on an object file not in an archive
#define SRC_LIBRARY        4 // Seen in an archive
#define SRC_SHARED_LIBRARY 8 // Seen in a library

#define SRC_OBJECT_OR_LIBRARY (SRC_OBJECT | SRC_LIBRARY)

// Symbol extra behavior for symbols based on relocations, ifuncs, etc.
#define SE_NONE            0
#define SE_IN_GOT          1
#define SE_IN_GOT_PLT      2
#define SE_IN_GOT_IPLT     4
#define SE_COPY_RELOCATION 8

#define GLOBAL_SYMBOL_INDEX_NONE    0
#define GLOBAL_SYMBOL_INDEX_DEFAULT 1

typedef struct symbol {
    char *name;                         // Name
    int version;                        // Version
    int binding;                        // Binding, e.g. local or global
    int type;                           // Type, e.g. function or object
    int other;                          // Visibility
    uint64_t size;
    int is_abs;                         // The src value is an absolute address
    int is_common;                      // The symbol is a common symbol. input_section is null.
    int sources;                        // A combination of at least one of SRC_*
    int extra;                          // One of SE_*
    int needs_dynsym_entry;             // Set to 1 if the symbol needs a dynsym entry. 0 doesn't exclude it from the .dynsym.
    uint64_t got_offset;                // Offset in the .got section, if present
    uint64_t got_plt_offset;            // Offset in the .got.plt section, if present
    uint64_t got_iplt_offset;           // Offset in the .got.iplt section, if present
    uint64_t plt_offset;                // Offset in the .plt section, if present
    uint64_t iplt_offset;               // Offset in the .iplt section, if present
    uint64_t rela_iplt_offset;          // Offset in the .rela.iplt section, if present
    InputElfFile *src_elf_file;         // File symbol is defined in. NULL if undefined.
    InputSection *input_section;        // Section the symbol is defined in. NULL if the symbol is undefined or common.
    OutputSection *output_section;
    uint64_t src_value;                 // Value in the original ELF section
    uint64_t dst_value;                 // Value in the final ELF section
    int dst_index;                      // Index in the final ELF symbol table
    int dst_dynsym_index;               // Index in the final ELF dynsyn table (for libraries)
    int resolves_undefined_symbol;      // Set to 1 if the symbol is from a shared library and resolves an undefined symbol in an object file
    int is_undefined;                   // Set to 1 at a later stage if the symbol is undefined
} Symbol;

typedef struct symbol_table {
    MapOrdered *defined_symbols;
    MapOrdered *undefined_symbols;
} SymbolTable;

typedef struct relative_rela_dyn_relocation {
    InputSection* target_section;               // Where the relocated value is written to
    InputSection* relocation_input_section;     // Either relocation_input_section or symbol must be not-null
    Symbol *symbol;                             // May not be set. Relocations in this case point to an offset in a section.
    uint64_t offset;                            // Relocation offset in the input section (if set)
    uint64_t addend;                            // Addend of the relocation
} RelativeRelaDynRelocation;

extern SymbolTable *global_symbol_table;
extern StrMapOrdered *local_symbol_tables;

extern char *last_error_message;

SymbolNV *new_symbolnv(const char *name, int version_index, int is_default, int is_proxy_for_default);
SymbolTable *new_symbol_table(void);
void init_symbols(OutputElfFile *output_elf_file);
Symbol *get_defined_symbol(SymbolTable *st, const char *name, int version_index);
Symbol *get_global_defined_symbol(const char *name, int version_index);
Symbol *must_get_defined_symbol(SymbolTable *st, const char *name, int version_index);
Symbol *must_get_global_defined_symbol(const char *name, int version_index);
SymbolTable *get_local_symbol_table(InputElfFile *elf_file);
Symbol *lookup_symbol(InputElfFile *elf_file, char *name, int version_index);
Symbol *new_symbol(const char *name, int type, int binding, int other, uint64_t size, int source);
Symbol *get_undefined_symbol(const char *name, int version_index);
int is_undefined_symbol(const char *name, int version_index);
Symbol *get_or_add_linker_script_symbol(CommandAssignment *assignment);
void debug_print_global_symbol_version_indexes();
int process_elf_file_symbols(InputElfFile *elf_file, int source, int read_only);
void resolve_provided_symbols(OutputElfFile *output_elf_file);
void finalize_symbols(OutputElfFile *output_elf_file);
void convert_hidden_symbols(OutputElfFile *output_elf_file);
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
void set_got_symbol_value(OutputElfFile *output_elf_file);
void create_got_symbol(OutputElfFile *output_elf_file);
void update_dynamic_relocatable_values(OutputElfFile *output_elf_file);
void process_ifuncs_from_symbol_table(OutputElfFile *output_elf_file, SymbolTable *symbol_table);
void allocate_extra_sections(OutputElfFile *output_elf_file);;
void update_iplt(OutputElfFile *output_elf_file);
void make_symbol_hashes(OutputElfFile *output_elf_file);
void create_dyn_rela_section(OutputElfFile *output_elf_file);
void update_dyn_rela_section(OutputElfFile *output_elf_file);
void layout_data_copy_section(OutputElfFile *output_elf_file);
void add_dynamic_symbol(OutputElfFile *output_elf_file);
void make_verneed_section(OutputElfFile *output_elf_file);
void make_versym_section(OutputElfFile *output_elf_file);

#endif
