#include <stdlib.h>

#include "error.h"
#include "strmap.h"

#include "wld/symbols.h"
#include "wld/wld.h"

StrMap *defined_symbols;    // Map of defined symbols, name -> symbol
StrMap *undefined_symbols;  // A set of undefined symbols

void init_symbols(void) {
    defined_symbols = new_strmap();
    undefined_symbols = new_strmap();
}

// Get a symbol from the defined symbol table. Returns NULL if not present.
Symbol *get_defined_symbol(char *name) {
    return (Symbol *) strmap_get(defined_symbols, name);
}

// Is a symbol in the undefined symbols set?
static int is_undefined_symbol(char *name) {
    return strmap_get(undefined_symbols, name) != NULL;
}

// Remove a symbol from the undefined symbols set
static void remove_undefined_symbol(char *name) {
    strmap_delete(undefined_symbols, name);
}

static Symbol *new_symbol(char *name, int type, int binding) {
    Symbol *symbol = calloc(1, sizeof(Symbol));

    symbol->name    = name;
    symbol->type    = type;
    symbol->binding = binding;

    return symbol;
}

static Symbol *add_defined_symbol(char *name, int type, int binding) {
    Symbol *symbol = new_symbol(name, type, binding);
    strmap_put(defined_symbols, name, symbol);
    return symbol;
}

static void add_undefined_symbol(char *name, int type, int binding) {
    strmap_put(undefined_symbols, name, (void *) 1);
}

void process_elf_file_symbols(ElfFile *elf_file) {
    for (int i = 0; i < elf_file->symbol_count; i++) {
        ElfSymbol *symbol = &elf_file->symbol_table[i];

        char binding = (symbol->st_info >> 4) & 0xf;
        char type = symbol->st_info & 0xf;

        if (!symbol->st_name) continue;

        int strtab_offset = symbol->st_name;
        char *name = &elf_file->strtab_strings[symbol->st_name];

        if (symbol->st_shndx == SHN_ABS) panic("SHN_ABS symbols aren't handled");
        if (symbol->st_shndx == SHN_COMMON) panic("SHN_COMMON symbols aren't handled");

        if (symbol->st_shndx == SHN_UNDEF) {
            Symbol *found_symbol = get_defined_symbol(name);
            if (!found_symbol)
                add_undefined_symbol(name, type, binding);
        }
        else {
            if (binding == STB_GLOBAL || binding == STB_WEAK) {
                Symbol *found_symbol = get_defined_symbol(name);

                if (found_symbol)  {
                    set_error_filename((char *) elf_file->filename);
                    set_error_line(0);
                    error_in_file("Multiple definition of %s\n", name);
                }
                else {
                    // Add a new symbol
                    Symbol *new_symbol = add_defined_symbol(name, type, binding);
                    new_symbol->src_elf_file = elf_file;
                    new_symbol->src_section = elf_file->section_list->elements[symbol->st_shndx];
                    new_symbol->src_value = symbol->st_value;

                    if (is_undefined_symbol(name))
                        remove_undefined_symbol(name);
                }
            }
        }
    }
}

void fail_on_undefined_symbols(void) {
    int count = 0;

    for (StrMapIterator it = strmap_iterator(undefined_symbols); !strmap_iterator_finished(&it); strmap_iterator_next(&it))
        count++;

    if (!count) return;

    printf("Undefined symbols:\n");
    for (StrMapIterator it = strmap_iterator(undefined_symbols); !strmap_iterator_finished(&it); strmap_iterator_next(&it)) {
        const char *name = strmap_iterator_key(&it);
        printf("  %s\n", name);
    }
    error("Unable to resolve undefined references");
}

void debug_summarize_symbols(void) {
    printf("Defined symbols:\n");
    printf("  %-40s %-40s %s %s\n", "Name", "Src file", "Src Value", "Dst Value");
    for (StrMapIterator it = strmap_iterator(defined_symbols); !strmap_iterator_finished(&it); strmap_iterator_next(&it)) {
        const char *name = strmap_iterator_key(&it);
        Symbol *symbol = strmap_get(defined_symbols, name);
        printf("  %-40s %-40s %08x  %08x\n", symbol->name, symbol->src_elf_file->filename, symbol->src_value, symbol->dst_value);
    }

    printf("Undefined symbols:\n");
    for (StrMapIterator it = strmap_iterator(undefined_symbols); !strmap_iterator_finished(&it); strmap_iterator_next(&it)) {
        const char *name = strmap_iterator_key(&it);
        printf("  %s\n", name);
    }
}

// Assign final values to all symbols
void make_symbol_values(uint64_t executable_virt_address) {
    if (DEBUG) printf("\nFinal symbols:\n");

    for (StrMapIterator it = strmap_iterator(defined_symbols); !strmap_iterator_finished(&it); strmap_iterator_next(&it)) {
        const char *name = strmap_iterator_key(&it);
        Symbol *symbol = strmap_get(defined_symbols, name);

        symbol->dst_value = executable_virt_address + symbol->src_section->dst_section->offset + symbol->src_section->offset + symbol->src_value;

        if (DEBUG) {
            printf("%-10s %-40s value=%08x  ", symbol->name, symbol->src_elf_file->filename, symbol->dst_value);
            printf("dst sec off %#0x sec off %#08x\n", symbol->src_section->dst_section->offset, symbol->src_section->offset);
        }
    }
}
