#ifndef _SYMBOLS_H
#define _SYMBOLS_H

#include "ro-elf.h"
#include "rw-elf.h"
#include "strmap.h"

typedef struct symbol {
    char *name;             // Name
    int binding;            // Binding, e.g. local or global
    int type;               // Type, e.g. function or object
    int other;              // Visibility
    int size;
    ElfFile *src_elf_file;  // File symbol is defined in. NULL if undefined.
    Section *src_section;   // Section the symbol is defined in. NULL if undefined.
    RwSection *dst_section; // Section the symbol is defined in. NULL if undefined.
    int src_value;          // Value in the original ELF section
    int dst_value;          // Value in the final ELF section
    int dst_index;          // Index in the final ELF symbol table
} Symbol;

extern StrMap *defined_symbols;
extern StrMap *undefined_symbols;

void init_symbols(void);
Symbol *get_defined_symbol(char *name);
int process_elf_file_symbols(ElfFile *elf_file, int read_only);
void fail_on_undefined_symbols(void);
void debug_print_symbol(Symbol *symbol);
void debug_summarize_symbols(void);
void make_symbol_values(RwElfFile *output_elf_file, uint64_t executable_virt_address);
void make_elf_symbols(RwElfFile *output_elf_file);
void update_elf_symbols(RwElfFile *output_elf_file);

#endif
