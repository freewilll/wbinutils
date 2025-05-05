#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "strmap.h"
#include "strmap-ordered.h"

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

SymbolTable *global_symbol_table;
StrMap *local_symbol_tables; // Map from filename to symbol table

char *last_error_message;

SymbolTable *new_symbol_table(void) {
    SymbolTable *st = malloc(sizeof(SymbolTable));
    st->defined_symbols = new_strmap_ordered();
    st->undefined_symbols = new_strmap_ordered();

    return st;
}

// Get a symbol from the defined symbol table. Returns NULL if not present.
Symbol *get_defined_symbol(SymbolTable *st, char *name) {
    return (Symbol *) strmap_ordered_get(st->defined_symbols, name);
}

// Get a symbol from the global defined symbol table. Returns NULL if not present.
Symbol *get_global_defined_symbol(char *name) {
    return get_defined_symbol(global_symbol_table, name);
}

// Get a symbol from the defined symbol table. Panic if it doesn't exist.
Symbol *must_get_defined_symbol(SymbolTable *st, char *name) {
    Symbol *symbol = get_defined_symbol(st, name);
    if (!symbol) panic("Expected a symbol %s, but got none", name);
    return symbol;
}

// Get a symbol from the global defined symbol table. Panic if it doesn't exist.
Symbol *must_get_global_defined_symbol(char *name) {
    return must_get_defined_symbol(global_symbol_table, name);
}

// Get an input elf file's local symbol table. Panic if it doesn't exist.
SymbolTable *get_local_symbol_table(ElfFile *elf_file) {
    SymbolTable *local_symbol_table = strmap_get(local_symbol_tables, elf_file->filename);
    if (!local_symbol_table) panic("Missing local symbol table for %s", elf_file->filename);
    return local_symbol_table;
}

// Get a symbol from either the local, otherwise the global defined symbol tables. Returns NULL if not present.
Symbol *lookup_symbol(ElfFile *elf_file, char *name) {
    SymbolTable *local_symbol_table = get_local_symbol_table(elf_file);
    Symbol *s = get_defined_symbol(local_symbol_table, name);
    if (s) return s;
    return get_defined_symbol(global_symbol_table, name);
}

// Get an undefined symbol. Returns NULL if not present.
static Symbol *get_undefined_symbol(char *name) {
    return strmap_ordered_get(global_symbol_table->undefined_symbols, name);
}

// Is a symbol in the undefined symbols set?
int is_undefined_symbol(char *name) {
    return get_undefined_symbol(name) != NULL;
}

// Remove a symbol from the undefined symbols set
static void remove_undefined_symbol(char *name) {
    if (DEBUG_SYMBOL_RESOLUTION) printf("  Removed undefined %s\n", name);
    strmap_ordered_delete(global_symbol_table->undefined_symbols, name);
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

static Symbol *add_defined_symbol(SymbolTable *st, char *name, int type, int binding, int other, int size, int is_library) {
    if (DEBUG_SYMBOL_RESOLUTION) printf("  Added defined symbol %s binding=%s\n", name, SYMBOL_BINDING_NAMES[binding]);
    Symbol *symbol = new_symbol(name, type, binding, other, size, is_library);
    strmap_ordered_put(st->defined_symbols, name, symbol);
    return symbol;
}

Symbol *get_or_add_linker_script_symbol(char *name) {
    Symbol *result = get_global_defined_symbol(name);
    if (result) return result;
    result = add_defined_symbol(global_symbol_table, name, STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, 0, 0);
    return result;
}

static void add_undefined_symbol(char *name, int type, int binding, int other, int size, int is_library) {
    if (DEBUG_SYMBOL_RESOLUTION) printf("  Added undefined symbol %s\n", name);
    Symbol *symbol = new_symbol(name, type, binding, other, size, is_library);
    strmap_ordered_put(global_symbol_table->undefined_symbols, name, symbol);
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

// Check if a symbol resolves an undefined symbol. If so and not read-only the undefined symbol is removed.
// Returns 1 if an undefined symbol has been resolved.
static int resolve_undefined_symbol(char *name, char binding, int is_library, int read_only) {
    Symbol *undefined_symbol = get_undefined_symbol(name);
    if (!undefined_symbol) return 0;

    // Resolve the undefined symbol.
    if (!read_only) remove_undefined_symbol(name);

    // Weak undefined symbols don't get resolved if also found in a library.
    if (undefined_symbol->binding == STB_WEAK && is_library && read_only) return 0;

    return 1;
}


static int handle_local_symbol(ElfFile *elf_file, SymbolTable *local_symbol_table, int is_library, int read_only, ElfSymbol *symbol) {
    int strtab_offset = symbol->st_name;
    char *name = &elf_file->strtab_strings[symbol->st_name];

    char binding = (symbol->st_info >> 4) & 0xf;
    char type = symbol->st_info & 0xf;
    int size = symbol->st_size;
    int other = symbol->st_other;

    Section *src_section = elf_file->section_list->elements[symbol->st_shndx];

    if (!read_only) {
        Symbol *new_symbol = add_defined_symbol(local_symbol_table, name, type, binding, other, size, is_library);
        new_symbol->src_elf_file = elf_file;
        new_symbol->src_section = src_section;
        new_symbol->src_value = symbol->st_value;
    }

    return resolve_undefined_symbol(name, binding, is_library, read_only);
}

// Handle a symbol where either the found symbol or symbol is common
// The found symbol may be undefined.
// Returns if the symbol has resolved an undefined symbol
// Not all the cases in https://www.airs.com/blog/archives/49 are handled, just the simple ones.
static int handle_common_symbol(ElfFile *elf_file, int is_library, int read_only, Symbol *found_symbol, ElfSymbol *symbol) {
    int result = 0;

    int strtab_offset = symbol->st_name;
    char *name = &elf_file->strtab_strings[symbol->st_name];

    char binding = (symbol->st_info >> 4) & 0xf;
    char type = symbol->st_info & 0xf;
    int size = symbol->st_size;
    int other = symbol->st_other;

    int is_common = symbol->st_shndx == SHN_COMMON;

    if (found_symbol) {
        if (!found_symbol->is_common && is_common) {
            // Not common - common
            return 0;
        }
        else if (found_symbol->is_common && !is_common) {
            // Common - not common
            // The new symbol takes over from the common symbol
            result = 1;

            if (!read_only) {
                Section *src_section = elf_file->section_list->elements[symbol->st_shndx];
                found_symbol->src_elf_file = elf_file;
                found_symbol->src_section = src_section;
                found_symbol->src_value = symbol->st_value;
                found_symbol->is_common = 0;
            }

            return 0;
        }
        else {
            // Common - common
            if (symbol->st_size > found_symbol->size) {
                found_symbol->size = symbol->st_size;
                return 1;
            }
        }
    }
    else {
        // The symbol has not yet been defined
        if (!read_only) {
            // Add a new symbol
            Symbol *new_symbol = add_defined_symbol(global_symbol_table, name, type, binding, other, size, is_library);
            new_symbol->src_elf_file = elf_file;
            new_symbol->src_section = NULL;
            new_symbol->src_value = symbol->st_value;
            new_symbol->is_common = 1;
        }

        result = resolve_undefined_symbol(name, binding, is_library, read_only);
    }

    return result;
}

// The new symbol is ABS
// Not very much is checked here, only undefined symbols are resolved.
static int handle_abs_symbol(ElfFile *elf_file, int is_library, int read_only, Symbol *found_symbol, ElfSymbol *symbol) {
    int strtab_offset = symbol->st_name;
    char *name = &elf_file->strtab_strings[symbol->st_name];

    char binding = (symbol->st_info >> 4) & 0xf;
    char type = symbol->st_info & 0xf;
    int size = symbol->st_size;
    int other = symbol->st_other;
    int value = symbol->st_value;

    if (found_symbol) {
        if (found_symbol->src_value != value)
            fail(elf_file, "Conflicting values of absolute symbol %s: %d and %d", name, found_symbol->src_value, value);
    }

    else {
        // The symbol has not yet been defined
        if (!read_only) {
            // Add a new symbol
            Symbol *new_symbol = add_defined_symbol(global_symbol_table, name, type, binding, other, size, is_library);
            new_symbol->src_elf_file = elf_file;
            new_symbol->src_section = NULL;
            new_symbol->src_value = symbol->st_value;
            new_symbol->is_abs = 1;
        }

        return resolve_undefined_symbol(name, binding, is_library, read_only);
    }

    return 0;
}

// Handle a symbol where the left side is defined. Both are not common.
// Returns if the symbol has resolved an undefined symbol
static int handle_non_common_symbol(ElfFile *elf_file, int is_library, int read_only, Symbol *found_symbol, ElfSymbol *symbol) {
    int result = 0;

    int strtab_offset = symbol->st_name;
    char *name = &elf_file->strtab_strings[symbol->st_name];

    char binding = (symbol->st_info >> 4) & 0xf;
    char type = symbol->st_info & 0xf;
    int size = symbol->st_size;
    int other = symbol->st_other;

    Section *src_section = elf_file->section_list->elements[symbol->st_shndx];

    if (found_symbol)  {
        // Check bindings

        // Two strong bindings
        if (found_symbol->binding != STB_WEAK && binding != STB_WEAK) {
            // Anecdotal mimic of what gcc does. If the second symbol is an object file,
            // It is an error.
            if (!is_library) fail(elf_file, "Multiple definition of %s", name);

            // The second symbol is ignored
        }

        // For weak-weak it's first come first served. Use the existing symbol
        else if (found_symbol->binding == STB_WEAK && binding == STB_WEAK)
            ; // Do nothing
        else {
            // One is strong and one is weak.
            // If the new symbol is strong and not in a library, it resolves the exiting weak one
            if (binding != STB_WEAK && !is_library) {
                // The new symbol is strong and takes over
                result = 1;

                if (!read_only) {
                    found_symbol->src_elf_file = elf_file;
                    found_symbol->src_section = src_section;
                    found_symbol->src_value = symbol->st_value;
                    found_symbol->is_common = 0;
                    found_symbol->binding = binding;
                }
            }
        }
    }
    else {
        // The symbol has not yet been defined
        if (!read_only) {
            // Add a new symbol
            Symbol *new_symbol = add_defined_symbol(global_symbol_table, name, type, binding, other, size, is_library);
            new_symbol->src_elf_file = elf_file;
            new_symbol->src_section = src_section;
            new_symbol->src_value = symbol->st_value;
            new_symbol->is_common = 0;
        }

        result = resolve_undefined_symbol(name, binding, is_library, read_only);
    }

    return result;
}

// Process all symbols in a file. Returns the amount of undefined symbols in
// the symbol table that would be resolved.
int process_elf_file_symbols(ElfFile *elf_file, int is_library, int read_only) {
    last_error_message = NULL;
    int resolved_symbols = 0;

    SymbolTable *local_symbol_table = new_symbol_table();

    if (!read_only) strmap_put(local_symbol_tables, elf_file->filename, local_symbol_table);

    for (int i = 0; i < elf_file->symbol_count; i++) {
        ElfSymbol *symbol = &elf_file->symbol_table[i];

        char binding = (symbol->st_info >> 4) & 0xf;
        char type = symbol->st_info & 0xf;
        int size = symbol->st_size;
        int other = symbol->st_other;

        if (!symbol->st_name) continue;
        int strtab_offset = symbol->st_name;
        char *name = &elf_file->strtab_strings[symbol->st_name];

        int is_local = binding == STB_LOCAL;

        if (type == STT_FILE) continue;

        Section *src_section = NULL;
        int is_abs = symbol->st_shndx == SHN_ABS;
        int is_common = symbol->st_shndx == SHN_COMMON;
        int is_undef = symbol->st_shndx == SHN_UNDEF;

        // Look up the src_section unless the symbol is common
        if (!is_abs && !is_common) {
            if (symbol->st_shndx >= elf_file->section_list->length)
                fail(elf_file, "Invalid section index in %s: %d >= %d",
                    elf_file->filename, symbol->st_shndx, elf_file->section_list->length);

            src_section = elf_file->section_list->elements[symbol->st_shndx];
        }

        if (is_local) {
            resolved_symbols += handle_local_symbol(elf_file, local_symbol_table, is_library, read_only, symbol);
        }
        else if (is_undef) {
            // The new symbol is undefined

            Symbol *found_symbol = get_defined_symbol(global_symbol_table, name);
            if (!found_symbol && !read_only) {
                // Add an undefined symbol unless it already exists
                if (!is_undefined_symbol(name)) {
                    add_undefined_symbol(name, type, binding, other, size, is_library);
                }
            }
        }
        else {
            // The new symbol is defined or common

            if (binding != STB_GLOBAL && binding != STB_WEAK)
                panic("Don't know how to handle a symbol with binding %d", binding);

            Symbol *found_symbol = get_defined_symbol(global_symbol_table, name);

            if ((found_symbol && found_symbol->is_common) || is_common)
                resolved_symbols += handle_common_symbol(elf_file, is_library, read_only, found_symbol, symbol);
            else if ((found_symbol && found_symbol->is_abs) || is_abs)
                resolved_symbols += handle_abs_symbol(elf_file, is_library, read_only, found_symbol, symbol);
            else
                resolved_symbols += handle_non_common_symbol(elf_file, is_library, read_only, found_symbol, symbol);
        }
    }

    return resolved_symbols;
}

// Treat all weak symbols as defined, with value zero. Fail if any undefined symbols are left
void finalize_symbols(void) {
    int count = 0;

    strmap_ordered_foreach(global_symbol_table->undefined_symbols, it) {
        const char *name = strmap_ordered_iterator_key(&it);
        Symbol *symbol = strmap_ordered_get(global_symbol_table->undefined_symbols, name);

        // Weak undefined symbols become defined with value zero
        if (symbol->binding == STB_WEAK) {
            symbol->dst_value = 0;
            strmap_ordered_put(global_symbol_table->defined_symbols, name, symbol);
        }
        else
            count++;
    }

    if (!count) return; // All symbols are defined

    printf("Undefined symbols:\n");
    strmap_ordered_foreach(global_symbol_table->undefined_symbols, it) {
        const char *name = strmap_ordered_iterator_key(&it);
        Symbol *symbol = strmap_ordered_get(global_symbol_table->undefined_symbols, name);
        if (symbol->binding == STB_WEAK) continue;

        printf("  %s\n", name);
    }

    error("Unable to resolve undefined references");
}

static char *print_section_string(Symbol *symbol) {
    RwSection *rw_section = symbol->dst_section;
    int rw_section_index = rw_section ? rw_section->index : 0;

    if (symbol->is_common)
        printf("COM");
    else
        printf("%3d", rw_section_index);
}

void debug_print_symbol(Symbol *symbol) {
    char binding = symbol->binding;
    char type = symbol->type;
    char visibility = symbol->other & 3;
    const char *type_name = SYMBOL_TYPE_NAMES[type];
    const char *binding_name = SYMBOL_BINDING_NAMES[binding];
    const char *visibility_name = SYMBOL_VISIBILITY_NAMES[visibility];

    printf("%016x  %4x %-8s%-7s%-7s  ", symbol->dst_value, symbol->size, type_name, binding_name, visibility_name);
    print_section_string(symbol);
    printf(" %s\n", symbol->name);
}

void debug_summarize_symbols(void) {
    printf("Defined symbols:\n");
    printf("   Num:    Value          Size Type    Bind   Vis        Ndx Name\n");

    int i = 0;
    strmap_ordered_foreach(global_symbol_table->defined_symbols, it) {
        const char *name = strmap_ordered_iterator_key(&it);
        Symbol *symbol = strmap_ordered_get(global_symbol_table->defined_symbols, name);

        char binding = symbol->binding;
        char type = symbol->type;
        char visibility = symbol->other & 3;
        const char *type_name = SYMBOL_TYPE_NAMES[type];
        const char *binding_name = SYMBOL_BINDING_NAMES[binding];
        const char *visibility_name = SYMBOL_VISIBILITY_NAMES[visibility];

        printf("%6d: %016x  %4x %-8s%-7s%-9s  ", i, symbol->dst_value, symbol->size, type_name, binding_name, visibility_name);
        print_section_string(symbol);
        printf(" %s\n", name);

        i++;
    }

    printf("Undefined symbols:\n");
    strmap_ordered_foreach(global_symbol_table->undefined_symbols, it) {
        const char *name = strmap_ordered_iterator_key(&it);
        Symbol *symbol = strmap_ordered_get(global_symbol_table->undefined_symbols, name);
        if (symbol->binding == STB_WEAK) continue; // It's ok for unresolved symbols to be weak
        debug_print_symbol(symbol);
    }
}

// Returns 1 if any defined symbols are common
int common_symbols_are_present(void) {
    strmap_ordered_foreach(global_symbol_table->defined_symbols, it) {
        const char *name = strmap_ordered_iterator_key(&it);
        Symbol *symbol = strmap_ordered_get(global_symbol_table->defined_symbols, name);
        if (symbol->is_common) return 1;
    }

    return 0;
}

// Returns 1 if any defined symbols are common
void layout_common_symbols_in_bss_section(RwSection *bss_section) {
    int offset = bss_section->size;
    int section_align = 1;

    strmap_ordered_foreach(global_symbol_table->defined_symbols, it) {
        const char *name = strmap_ordered_iterator_key(&it);
        Symbol *symbol = strmap_ordered_get(global_symbol_table->defined_symbols, name);
        if (!symbol->is_common) continue;

        int symbol_align = symbol->src_value;
        if (symbol_align > section_align) section_align = symbol_align;

        symbol->src_value = offset;
        symbol->dst_section = bss_section;

        offset = ALIGN_UP(offset + symbol->size, symbol_align);
    }

    bss_section->align = section_align;
    bss_section->size = offset;
}

// Assign final values to all symbols
void make_symbol_values_from_symbol_table(RwElfFile *output_elf_file, SymbolTable *symbol_table) {
    strmap_ordered_foreach(symbol_table->defined_symbols, it) {
        const char *name = strmap_ordered_iterator_key(&it);
        Symbol *symbol = strmap_ordered_get(symbol_table->defined_symbols, name);

        if (symbol->is_abs) {
            // Do nothing, it's already resolved
        }
        else if (symbol->is_common) {
            RwSection *section_bss = output_elf_file->section_bss;
            symbol->dst_value = section_bss->address + symbol->src_value;
            symbol->dst_section = section_bss;
        }
        else {
            // Skip weak undefined symbols that don't have a src_section
            if (symbol->src_section) {
                // Get the output section
                if (!symbol->src_section->dst_section) panic("Unexpectedly got null dst_section for %s from %s\n", symbol->name, symbol->src_section->name);

                RwSection *rw_section = symbol->src_section->dst_section;
                if (!rw_section) panic("Unexpected empty output section for %s", name);
                symbol->dst_section = rw_section;

                if (!symbol->src_section)
                    panic("Unexpected null symbol->src_section for %s", name);
                if (!symbol->src_section->dst_section)
                    panic("Unexpected null symbol->src_section->dst_section for symbol %s in section %s", name, symbol->src_section->name);

                if (symbol->type == STT_TLS)
                    symbol->dst_value = symbol->src_section->dst_section->offset + symbol->src_section->offset + symbol->src_value - output_elf_file->tls_template_offset;
                else
                    symbol->dst_value = symbol->src_section->dst_section->address + symbol->src_section->offset + symbol->src_value;
            }

            if (DEBUG_RELOCATIONS) {
                printf("%-10s %-40s value=%08x  ", symbol->name, symbol->src_elf_file ? symbol->src_elf_file->filename : "-", symbol->dst_value);

                if (symbol->binding != STB_WEAK)
                    printf("dst sec off %#0x sec off %#08x\n", symbol->src_section->dst_section->offset, symbol->src_section->offset);
                else
                    printf("\n");
            }
        }
    }
}

// Add the symbols to the ELF symbol table
// The values and section indexes will be updated later
void make_elf_symbols(RwElfFile *output_elf_file) {
    ElfSectionHeader *elf_section_header = &output_elf_file->elf_section_headers[output_elf_file->section_symtab->index];

    output_elf_file->section_symtab->link = output_elf_file->section_strtab->index;
    output_elf_file->section_symtab->info = output_elf_file->sections_list->length; // Index of the first global symbol

    add_elf_symbol(output_elf_file, "", 0, 0, STB_LOCAL, STT_NOTYPE, STV_DEFAULT, SHN_UNDEF); // Null symbol

    for (int i = 1; i < output_elf_file->sections_list->length; i++) {
        RwSection *section = output_elf_file->sections_list->elements[i];
        add_elf_symbol(output_elf_file, "", 0, 0, STB_LOCAL, STT_SECTION, STV_DEFAULT, i);
    }

    strmap_ordered_foreach(global_symbol_table->defined_symbols, it) {
        const char *name = strmap_ordered_iterator_key(&it);
        Symbol *symbol = strmap_ordered_get(global_symbol_table->defined_symbols, name);
        symbol->dst_index = add_elf_symbol(output_elf_file, symbol->name, 0, symbol->size, symbol->binding, symbol->type, symbol->visibility, 0);
    }
}

// Set the symbol's value and section indexes
void update_elf_symbols(RwElfFile *output_elf_file) {
    strmap_ordered_foreach(global_symbol_table->defined_symbols, it) {
        const char *name = strmap_ordered_iterator_key(&it);
        Symbol *symbol = strmap_ordered_get(global_symbol_table->defined_symbols, name);
        if (!symbol->dst_section) continue; // Weak symbols may not be defined
        ElfSymbol *elf_symbols = (ElfSymbol *) output_elf_file->section_symtab->data;
        ElfSymbol *elf_symbol = &elf_symbols[symbol->dst_index];
        elf_symbol->st_value = symbol->dst_value;
        elf_symbol->st_shndx = symbol->dst_section->index;
    }
}

static void add_hidden_symbol(char *name) {
    Symbol *symbol = add_defined_symbol(global_symbol_table, name, STT_NOTYPE, STB_GLOBAL, 0, 0, 0);
    symbol->is_abs = 1;
    symbol->visibility = STV_HIDDEN;
}

void init_symbols(void) {
    global_symbol_table = new_symbol_table();
    local_symbol_tables = new_strmap();

    add_hidden_symbol(GLOBAL_OFFSET_TABLE_SYMBOL_NAME);
    add_hidden_symbol(PREINIT_ARRAY_START_SYMBOL_NAME);
    add_hidden_symbol(PREINIT_ARRAY_END_SYMBOL_NAME);
    add_hidden_symbol(INIT_ARRAY_START_SYMBOL_NAME);
    add_hidden_symbol(INIT_ARRAY_END_SYMBOL_NAME);
    add_hidden_symbol(FINI_ARRAY_START_SYMBOL_NAME);
    add_hidden_symbol(FINI_ARRAY_END_SYMBOL_NAME);
}

// Create the .got section, if needed
void create_global_offset_table(RwElfFile *output_elf_file) {
    // Scan the symbol table and count the amount of GOT entries
    int got_entries_count = 0;
    strmap_ordered_foreach(global_symbol_table->defined_symbols, it) {
        const char *name = strmap_ordered_iterator_key(&it);
        Symbol *symbol = strmap_ordered_get(global_symbol_table->defined_symbols, name);
        if (symbol->needs_got) got_entries_count++;
    }

    if (!got_entries_count) return;


    // Use a pre-existing .got from the linker script if already present
    output_elf_file->section_got = get_rw_section(output_elf_file, ".got");

    if (output_elf_file->section_got) {
        if (output_elf_file->section_got->size > 0)
            // If an existing .got exists, then the entries need merging with the ones discovered here.
            panic("Dealing with existing .got entries isn't implemented");

        output_elf_file->section_got->type = SHT_PROGBITS;
        output_elf_file->section_got->flags = SHF_ALLOC | SHF_WRITE;
        output_elf_file->section_got->align = 8;
    }
    else  {
        // Create an empty .got section
        output_elf_file->section_got = add_rw_section(output_elf_file, ".got", SHT_PROGBITS, SHF_ALLOC | SHF_WRITE, 8);
    }

    output_elf_file->section_got->size = 8 * got_entries_count;
    output_elf_file->section_got->data = calloc(1, output_elf_file->section_got->size);
}

// Set the symbol values in the GOT (if present)
void update_got_symbol_values(RwElfFile *output_elf_file) {
    if (!output_elf_file->section_got) return; // No GOT, do nothing

    // Set the special GOT symbol to the virtual address of the GOT
    Symbol *got_symbol = must_get_global_defined_symbol(GLOBAL_OFFSET_TABLE_SYMBOL_NAME);
    got_symbol->dst_value = output_elf_file->section_got->address;

    // Set the address of the GOT
    output_elf_file->got_virt_address = got_symbol->dst_value;

    // Add the GOT entries
    uint64_t *got_entries = (uint64_t *) output_elf_file->section_got->data;

    int i = 0;
    strmap_ordered_foreach(global_symbol_table->defined_symbols, it) {
        const char *name = strmap_ordered_iterator_key(&it);
        Symbol *symbol = strmap_ordered_get(global_symbol_table->defined_symbols, name);
        if (!symbol->needs_got) continue;

        if (i * 8 >= output_elf_file->section_got ->size) panic("Trying to write beyond the allocated space in the GOT");
        got_entries[i] = symbol->dst_value;
        symbol->got_offset = i * 8;

        i++;
    }
}
