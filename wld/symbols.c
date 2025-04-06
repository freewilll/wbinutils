#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "strmap.h"

#include "wld/symbols.h"
#include "wld/wld.h"

static const char *SYMBOL_TYPE_NAMES[] = {
    "NOTYPE", "OBJECT", "FUNC", "SECTION", "FILE", "COMMON", "?", "?",
    "?", "?", "?", "?", "?", "?", "?", "?"
};

static const char *SYMBOL_BINDING_NAMES[] = {
    "LOCAL", "GLOBAL", "WEAK", "?", "?", "?", "?", "?",
    "?", "?", "?", "?", "?", "?", "?", "?",
};

static const char *SYMBOL_VISIBILITY_NAMES[] = {
    "DEFAULT", "INTERNAL", "HIDDEN", "PROTECTED",
};

StrMap *defined_symbols;    // Map of defined symbols, name -> symbol
StrMap *undefined_symbols;  // A set of undefined symbols

char *last_error_message;

void init_symbols(void) {
    defined_symbols = new_strmap();
    undefined_symbols = new_strmap();
}

// Get a symbol from the defined symbol table. Returns NULL if not present.
Symbol *get_defined_symbol(char *name) {
    return (Symbol *) strmap_get(defined_symbols, name);
}

Symbol *must_get_defined_symbol(char *name) {
    Symbol *symbol = get_defined_symbol(name);
    if (!symbol) panic("Expected a symbol %s, but got none", name);
    return symbol;
}

// Is a symbol in the undefined symbols set?
static int is_undefined_symbol(char *name) {
    return strmap_get(undefined_symbols, name) != NULL;
}

// Remove a symbol from the undefined symbols set
static void remove_undefined_symbol(char *name) {
    strmap_delete(undefined_symbols, name);
}

static Symbol *new_symbol(char *name, int type, int binding, int other, int size, int is_library) {
    Symbol *symbol = calloc(1, sizeof(Symbol));

    symbol->name           = name;
    symbol->type           = type;
    symbol->binding        = binding;
    symbol->other          = other;
    symbol->size           = size;
    symbol->src_is_library = is_library;

    return symbol;
}

static Symbol *add_defined_symbol(char *name, int type, int binding, int other, int size, int is_library) {
    Symbol *symbol = new_symbol(name, type, binding, other, size, is_library);
    strmap_put(defined_symbols, name, symbol);
    return symbol;
}

static void add_undefined_symbol(char *name, int type, int binding, int other, int size, int is_library) {
    Symbol *symbol = new_symbol(name, type, binding, other, size, is_library);
    strmap_put(undefined_symbols, name, symbol);
}

// Report an error, or in case we are being tested, write to last_error_message
static void fail(ElfFile *elf_file, char *format, ...) {
    va_list ap;
    va_start(ap, format);
    va_list ap2;
    va_copy(ap2, ap);

    // Test if the filename is a magic string
    if (!strcmp(elf_file->filename, "/_testing.o")) {
        int size = vsnprintf(NULL, 0, format, ap);
        last_error_message = malloc(size + 1);
        vsprintf(last_error_message, format, ap2);
        return;
    }

    set_error_filename((char *) elf_file->filename);
    set_error_line(0);
    verror_in_file(format, ap);
}

// Process all symbols in a file. Returns the amount of undefined symbols in
// the symbol table that would be resolved.
int process_elf_file_symbols(ElfFile *elf_file, int is_library, int read_only) {
    last_error_message = NULL;
    int resolved_symbols = 0;

    for (int i = 0; i < elf_file->symbol_count; i++) {
        ElfSymbol *symbol = &elf_file->symbol_table[i];

        char binding = (symbol->st_info >> 4) & 0xf;
        char type = symbol->st_info & 0xf;
        int size = symbol->st_size;
        int other = symbol->st_other;

        if (!symbol->st_name) continue;

        int strtab_offset = symbol->st_name;
        char *name = &elf_file->strtab_strings[symbol->st_name];

        if (type == STT_FILE) continue;

        if (symbol->st_shndx == SHN_ABS) panic("SHN_ABS symbols aren't handled");
        if (symbol->st_shndx == SHN_COMMON) panic("SHN_COMMON symbols aren't handled");

        if (symbol->st_shndx == SHN_UNDEF) {
            Symbol *found_symbol = get_defined_symbol(name);
            if (!found_symbol && !read_only) {
                // Add an undefined symbol unless it already exists
                if (!is_undefined_symbol(name)) {
                    add_undefined_symbol(name, type, binding, other, size, is_library);
                }
            }
        }
        else {
            if (binding == STB_GLOBAL || binding == STB_WEAK) {
                Symbol *found_symbol = get_defined_symbol(name);

                if (found_symbol)  {
                    // Check bindings

                    // Two strong bindings
                    if (found_symbol->binding != STB_WEAK && binding != STB_WEAK) {
                        // Anecdotal mimic of what gcc does. If the second symbol is an object file,
                        // It is an error.
                        if (!is_library) {
                            fail(elf_file, "Multiple definition of %s", name);
                        }

                        // The second symbol is ignored

                        resolved_symbols++;
                    }

                    // For weak-weak it's first come first served. Use the existing symbol
                    else if (found_symbol->binding == STB_WEAK && binding == STB_WEAK)
                        ; // Do nothing
                    else {
                        // One is strong and one is weak.
                        if (binding != STB_WEAK) {
                            // The new symbol is strong and takes over
                            resolved_symbols++;

                            if (!read_only) {
                                found_symbol->src_elf_file = elf_file;
                                found_symbol->src_section = elf_file->section_list->elements[symbol->st_shndx];
                                found_symbol->src_value = symbol->st_value;
                            }
                        }
                    }
                }
                else {
                    // The symbol has not yet been defined

                    if (!read_only) {
                        // Add a new symbol
                        Symbol *new_symbol = add_defined_symbol(name, type, binding, size, other, is_library);
                        new_symbol->src_elf_file = elf_file;
                        new_symbol->src_section = elf_file->section_list->elements[symbol->st_shndx];
                        new_symbol->src_value = symbol->st_value;
                    }

                    if (is_undefined_symbol(name)) {
                        // This resolved an undefined symbol
                        if (!read_only) remove_undefined_symbol(name);
                        resolved_symbols++;
                    }
                }
            }
        }
    }

    return resolved_symbols;
}

void fail_on_undefined_symbols(void) {
    int count = 0;

    for (StrMapIterator it = strmap_iterator(undefined_symbols); !strmap_iterator_finished(&it); strmap_iterator_next(&it)) {
        const char *name = strmap_iterator_key(&it);
        Symbol *symbol = strmap_get(undefined_symbols, name);

        // It's ok for unresolved symbols to be weak. They just get value zero.
        if (symbol->binding == STB_WEAK) continue;
        count++;
    }

    if (!count) return;

    printf("Undefined symbols:\n");
    for (StrMapIterator it = strmap_iterator(undefined_symbols); !strmap_iterator_finished(&it); strmap_iterator_next(&it)) {
        const char *name = strmap_iterator_key(&it);
        Symbol *symbol = strmap_get(undefined_symbols, name);

        // It's ok for unresolved symbols to be weak. They just get value zero.
        if (symbol->binding == STB_WEAK) continue;
        printf("  %s\n", name);
    }

    error("Unable to resolve undefined references");
}

void debug_print_symbol(Symbol *symbol) {
    char binding = symbol->binding;
    char type = symbol->type;
    char visibility = symbol->other & 3;
    const char *type_name = SYMBOL_TYPE_NAMES[type];
    const char *binding_name = SYMBOL_BINDING_NAMES[binding];
    const char *visibility_name = SYMBOL_VISIBILITY_NAMES[visibility];

    RwSection *rw_section = symbol->dst_section;
    int rw_section_index = rw_section ? rw_section->index : 0;

    printf("%016x  %4x %-8s%-7s%-7s  ", symbol->dst_value, symbol->size, type_name, binding_name, visibility_name);
    printf("%3d", rw_section_index);
    printf(" %s\n", symbol->name);
}

void debug_summarize_symbols(void) {
    printf("Defined symbols:\n");
    printf("   Num:    Value          Size Type    Bind   Vis        Ndx Name\n");

    int i = 0;
    for (StrMapIterator it = strmap_iterator(defined_symbols); !strmap_iterator_finished(&it); strmap_iterator_next(&it)) {
        const char *name = strmap_iterator_key(&it);
        Symbol *symbol = strmap_get(defined_symbols, name);

        char binding = symbol->binding;
        char type = symbol->type;
        char visibility = symbol->other & 3;
        const char *type_name = SYMBOL_TYPE_NAMES[type];
        const char *binding_name = SYMBOL_BINDING_NAMES[binding];
        const char *visibility_name = SYMBOL_VISIBILITY_NAMES[visibility];

        RwSection *rw_section = symbol->dst_section;
        int rw_section_index = rw_section ? rw_section->index : 0;

        printf("%6d: %016x  %4x %-8s%-7s%-9s  ", i, symbol->dst_value, symbol->size, type_name, binding_name, visibility_name);
        printf("%3d", rw_section_index);
        printf(" %s\n", name);

        i++;
    }

    printf("Undefined symbols:\n");
    for (StrMapIterator it = strmap_iterator(undefined_symbols); !strmap_iterator_finished(&it); strmap_iterator_next(&it)) {
        const char *name = strmap_iterator_key(&it);
        Symbol *symbol = strmap_get(undefined_symbols, name);
        if (symbol->binding == STB_WEAK) continue; // It's ok for unresolved symbols to be weak
        debug_print_symbol(symbol);
    }
}

// Assign final values to all symbols
void make_symbol_values(RwElfFile *output_elf_file, uint64_t executable_virt_address) {
    if (DEBUG) printf("\nFinal symbols:\n");

    for (StrMapIterator it = strmap_iterator(defined_symbols); !strmap_iterator_finished(&it); strmap_iterator_next(&it)) {
        const char *name = strmap_iterator_key(&it);
        Symbol *symbol = strmap_get(defined_symbols, name);

        if (!symbol->src_section) panic("Unexpected null symbol->src_section for %s", name);
        if (!symbol->src_section->dst_section) panic("Unexpected null symbol->src_section->dst_section for symbol %s in section %s", name, symbol->src_section->name);

        symbol->dst_value = executable_virt_address + symbol->src_section->dst_section->offset + symbol->src_section->offset + symbol->src_value;

        // Get the output section
        RwSection *rw_section = get_rw_section(output_elf_file, symbol->src_section->dst_section->name);
        if (!rw_section) panic("Unexpected empty output section for %s", name);
        symbol->dst_section = rw_section;

        if (DEBUG) {
            printf("%-10s %-40s value=%08x  ", symbol->name, symbol->src_elf_file->filename, symbol->dst_value);
            printf("dst sec off %#0x sec off %#08x\n", symbol->src_section->dst_section->offset, symbol->src_section->offset);
        }
    }
}

// Add the symbols to the ELF symbol table
// The values and section indexes will be updated later
void make_elf_symbols(RwElfFile *output_elf_file) {
    ElfSectionHeader *elf_section_header = &output_elf_file->elf_section_headers[output_elf_file->section_symtab->index];

    output_elf_file->section_symtab->link = output_elf_file->section_strtab->index;
    output_elf_file->section_symtab->info = output_elf_file->sections_list->length; // Index of the first global symbol

    add_elf_symbol(output_elf_file, "", 0, 0, STB_LOCAL, STT_NOTYPE, SHN_UNDEF); // Null symbol

    for (int i = 1; i < output_elf_file->sections_list->length; i++) {
        RwSection *section = output_elf_file->sections_list->elements[i];
        add_elf_symbol(output_elf_file, "", 0, 0, STB_LOCAL, STT_SECTION, i);
    }

    for (StrMapIterator it = strmap_iterator(defined_symbols); !strmap_iterator_finished(&it); strmap_iterator_next(&it)) {
        const char *name = strmap_iterator_key(&it);
        Symbol *symbol = strmap_get(defined_symbols, name);
        symbol->dst_index = add_elf_symbol(output_elf_file, symbol->name, 0, 0, STB_GLOBAL, STT_NOTYPE, 0);
    }
}

// Set the symbol's value and section indexes
void update_elf_symbols(RwElfFile *output_elf_file) {
    for (StrMapIterator it = strmap_iterator(defined_symbols); !strmap_iterator_finished(&it); strmap_iterator_next(&it)) {
        const char *name = strmap_iterator_key(&it);
        Symbol *symbol = strmap_get(defined_symbols, name);
        ElfSymbol *elf_symbols = (ElfSymbol *) output_elf_file->section_symtab->data;
        ElfSymbol *elf_symbol = &elf_symbols[symbol->dst_index];
        elf_symbol->st_value = symbol->dst_value;
        elf_symbol->st_shndx = symbol->dst_section->index;
    }
}
