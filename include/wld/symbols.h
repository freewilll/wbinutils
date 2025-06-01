#ifndef _SYMBOLS_H
#define _SYMBOLS_H

#include "ro-elf.h"
#include "rw-elf.h"
#include "strmap.h"
#include "strmap-ordered.h"

#include "wld/script.h"

#define GLOBAL_OFFSET_TABLE_SYMBOL_NAME     "_GLOBAL_OFFSET_TABLE_"

typedef struct symbol {
    char *name;                 // Name
    int binding;                // Binding, e.g. local or global
    int type;                   // Type, e.g. function or object
    int other;                  // Visibility
    int size;
    int is_abs;                 // The src value is an absolute address
    int is_common;              // The symbol is a common symbol. src_section is null.
    int visibility;             // Used by the linker
    int src_is_library;         // 1 if the symbol was found in a library, otherwise it was found in an object file
    int needs_got;              // Set if the symbol needs an entry in the Global Offset Table (GOT)
    int needs_got_iplt;         // Set if the symbol needs an entry in the .got.iplt table, for ifuncs
    uint64_t got_offset;        // Offset in the .got section, if present
    uint64_t got_iplt_offset;   // Offset in the .got.iplt section, if present
    uint64_t iplt_offset;       // Offset in the .iplt section, if present
    uint64_t rela_iplt_offset;  // Offset in the .rela.iplt section, if present
    ElfFile *src_elf_file;      // File symbol is defined in. NULL if undefined.
    Section *src_section;       // Section the symbol is defined in. NULL if the symbol is undefined or common.
    RwSection *dst_section;
    int src_value;              // Value in the original ELF section
    int dst_value;              // Value in the final ELF section
    int dst_index;              // Index in the final ELF symbol table
} Symbol;

typedef struct symbol_table {
    StrMapOrdered *defined_symbols;
    StrMapOrdered *undefined_symbols;
} SymbolTable;

extern SymbolTable *global_symbol_table;
extern StrMap *local_symbol_tables;

extern char *last_error_message;

SymbolTable *new_symbol_table(void);
void init_symbols(void);
Symbol *get_defined_symbol(SymbolTable *st, char *name);
Symbol *get_global_defined_symbol(char *name);
Symbol *must_get_defined_symbol(SymbolTable *st, char *name);
Symbol *must_get_global_defined_symbol(char *name);
SymbolTable *get_local_symbol_table(ElfFile *elf_file);
Symbol *lookup_symbol(ElfFile *elf_file, char *name);
Symbol *get_undefined_symbol(const char *name);
int is_undefined_symbol(char *name);
Symbol *get_or_add_linker_script_symbol(CommandAssignment *assignment);
int process_elf_file_symbols(ElfFile *elf_file, int is_library, int read_only);
void finalize_symbols(void);
void dump_rw_symbols(RwElfFile *output_elf_file);
void debug_print_symbol(Symbol *symbol);
void debug_summarize_symbols(void);
int common_symbols_are_present(void);
void layout_common_symbols_in_bss_section(RwSection *bss_section);
void make_symbol_values_from_symbol_table(RwElfFile *output_elf_file, SymbolTable *symbol_table);
void make_elf_symbols(RwElfFile *output_elf_file);
void update_elf_symbols(RwElfFile *output_elf_file);
void create_got_section(RwElfFile *output_elf_file);
void update_got_symbol_values(RwElfFile *output_elf_file);
void process_ifuncs_from_symbol_table(RwElfFile *output_elf_file, SymbolTable *symbol_table);
void allocate_extra_sections(RwElfFile *output_elf_file);;
void update_iplt(RwElfFile *output_elf_file);
#endif
