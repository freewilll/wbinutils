#ifndef _SYMBOLS_H
#define _SYMBOLS_H

#include "ro-elf.h"
#include "strmap.h"

typedef struct symbol {
    char *name;             // Name
    int binding;            // Binding, e.g. local or global
    int type;               // Type, e.g. function or object
    ElfFile *src_elf_file;  // File symbol is defined in. NULL if undefined.
    Section *src_section;   // Section the symbol is defined in. NULL if undefined.
    int src_value;          // Value in the original ELF section
    int dst_value;          // Value in the final ELF section
} Symbol;

extern StrMap *defined_symbols;
extern StrMap *undefined_symbols;

void init_symbols(void);
void process_elf_file_symbols(ElfFile *elf_file);
void fail_on_undefined_symbols(void);
void debug_summarize_symbols(void);

#endif
