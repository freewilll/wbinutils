#ifndef _RELOCATIONS_H
#define _RELOCATIONS_H

#include "rw-elf.h"
#include "symbols.h"

typedef struct relocation {
    Symbol *symbol;         // Symbol the relocation gets its offset from
    int type;               // Type of relocation
    int offset;             // Offset in the data section the relocated address ends up in
    int addend;             // Number to add to the symbol
    RwSection *section;     // RwSection the relocation applies to
    int size;               // Redundant, since type covers it, but still useful.
} Relocation;

void init_relocations(void);
RwSection *get_relocation_section(RwSection *section);
void add_relocation(RwSection *section, Symbol *symbol, int type, long offset, int addend);
void add_elf_relocations(void);
void make_rela_sections(void);

#endif
