#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "map-ordered.h"
#include "strmap.h"
#include "strmap-ordered.h"

#include "wld/symbols.h"
#include "wld/script.h"
#include "wld/wld.h"

unsigned int symbol_nv_hash(const void *ptr) {
    const SymbolNV *key = ptr;
    unsigned int hash = 2166136261u;
    for (const char *p = key->name; *p; p++)
        hash = (hash ^ (unsigned char)*p) * 16777619;
    return (hash ^ key->version_index) * 16777619;
}

int symbol_nv_compare(const void *a, const void *b) {
    const SymbolNV *snva = a;
    const SymbolNV *snvb = b;
    int cmp = strcmp(snva->name, snvb->name);
    if (cmp) return cmp;
    return (snva->version_index - snvb->version_index);
}

#define SYMBOL_IS_IN_DYNSYM(symbol) (strcmp((symbol)->name, ".") && ((symbol)->binding == STB_GLOBAL || (symbol)->binding == STB_WEAK || is_undefined_symbol((symbol)->name)))

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
StrMapOrdered *provided_symbols; // symbols from linker script PROVIDE() and PROVIDE_HIDDEN. Map from symbol name to Symbol

char *last_error_message;

static SymbolNV make_symbolnv(const char *name, int version_index) {
    SymbolNV snv;
    snv.name = name;
    snv.version_index = version_index;

    return snv;
}

static SymbolNV *new_symbolnv(const char *name, int version_index) {
    SymbolNV *snv = malloc(sizeof(SymbolNV));
    snv->name = strdup(name);
    snv->version_index = version_index;
}

SymbolTable *new_symbol_table(void) {
    SymbolTable *st = malloc(sizeof(SymbolTable));
    st->defined_symbols = new_map_ordered(symbol_nv_hash, symbol_nv_compare);
    st->undefined_symbols = new_map_ordered(symbol_nv_hash, symbol_nv_compare);

    return st;
}

// Get a symbol from the defined symbol table. Returns NULL if not present.
Symbol *get_defined_symbol(SymbolTable *st, char *name) {
    int version_index = 0; // TODO
    SymbolNV snv = make_symbolnv(name, version_index);
    return (Symbol *) map_ordered_get(st->defined_symbols, &snv);
}

// Get a symbol from the global defined symbol table. Returns NULL if not present.
Symbol *get_global_defined_symbol(char *name) {
    return get_defined_symbol(global_symbol_table, name);
}

// Get a symbol from the defined symbol table. Panic if it doesn't exist.
Symbol *must_get_defined_symbol(SymbolTable *st, char *name) {
    Symbol *symbol = get_defined_symbol(st, name);
    if (!symbol) panic("Symbol not defined %s", name);
    return symbol;
}

// Get a symbol from the global defined symbol table. Panic if it doesn't exist.
Symbol *must_get_global_defined_symbol(char *name) {
    return must_get_defined_symbol(global_symbol_table, name);
}

// Get an input elf file's local symbol table. Panic if it doesn't exist.
SymbolTable *get_local_symbol_table(InputElfFile *elf_file) {
    SymbolTable *local_symbol_table = strmap_get(local_symbol_tables, elf_file->filename);
    if (!local_symbol_table) panic("Missing local symbol table for %s", elf_file->filename);
    return local_symbol_table;
}

// Get a symbol from either the local, otherwise the global defined symbol tables. Returns NULL if not present.
Symbol *lookup_symbol(InputElfFile *elf_file, char *name) {
    SymbolTable *local_symbol_table = get_local_symbol_table(elf_file);
    Symbol *s = get_defined_symbol(local_symbol_table, name);
    if (s) return s;
    return get_defined_symbol(global_symbol_table, name);
}

// Get an undefined symbol. Returns NULL if not present.
Symbol *get_undefined_symbol(const char *name, int version_index) {
    SymbolNV snv = make_symbolnv(name, version_index);
    return map_ordered_get(global_symbol_table->undefined_symbols, &snv);
}

// Is a symbol in the undefined symbols set?
int is_undefined_symbol(const char *name) {
    int version_index = 0; // TODO
    return get_undefined_symbol(name, version_index) != NULL;
}

// Remove a symbol from the undefined symbols set
static void remove_undefined_symbol(const char *name, int version_index) {
    if (DEBUG_SYMBOL_RESOLUTION) printf("  Removed undefined %s\n", name);
    SymbolNV snv = make_symbolnv(name, version_index);
    map_ordered_delete(global_symbol_table->undefined_symbols, &snv);
}

static Symbol *new_symbol(const char *name, int type, int binding, int other, uint64_t size, int is_library, int is_shared_library) {
    Symbol *symbol = calloc(1, sizeof(Symbol));

    symbol->name                  = strdup(name);
    symbol->type                  = type;
    symbol->binding               = binding;
    symbol->other                 = other;
    symbol->size                  = size;
    symbol->src_is_library        = is_library;
    symbol->src_is_shared_library = is_shared_library;

    return symbol;
}

static Symbol *add_defined_symbol(SymbolTable *st, const char *name, int type, int binding, int other, uint64_t size, int is_library, int is_shared_library) {
    if (DEBUG_SYMBOL_RESOLUTION) printf("  Added defined symbol %s binding=%s\n", name, SYMBOL_BINDING_NAMES[binding]);
    Symbol *symbol = new_symbol(name, type, binding, other, size, is_library, is_shared_library);
    int symbol_version = 0; // TODO
    SymbolNV *snv = new_symbolnv(name, symbol_version);
    map_ordered_put(st->defined_symbols, snv, symbol);

    return symbol;
}

// Check if a symbol resolves an undefined symbol. If so and not read-only the undefined symbol is removed.
// Returns 1 if an undefined symbol has been resolved.
static int resolve_undefined_symbol(const char *name, int is_library, int read_only) {
    int version_index = 0; // TODO
    Symbol *undefined_symbol = get_undefined_symbol(name, version_index);
    if (!undefined_symbol) return 0;

    // Resolve the undefined symbol.
    if (!read_only) remove_undefined_symbol(name, version_index);

    // Weak undefined symbols don't get resolved if also found in a library.
    if (undefined_symbol->binding == STB_WEAK && is_library && read_only) return 0;

    return 1;
}

static Symbol *add_undefined_symbol(char *name, int version_index, int type, int binding, int other, uint64_t size, int is_library) {
    if (DEBUG_SYMBOL_RESOLUTION) printf("  Added undefined symbol %s\n", name);
    Symbol *symbol = new_symbol(name, type, binding, other, size, is_library, 0);
    SymbolNV *snv = new_symbolnv(name, version_index);
    map_ordered_put(global_symbol_table->undefined_symbols, snv, symbol);

    return symbol;
}

// Return a defined symbol if it already exists.
// Otherwise, create one.
// Resolve an undefined symbol if there is one.
Symbol *get_or_add_linker_script_symbol(CommandAssignment *assignment) {
    char *name = strdup(assignment->name);

    Symbol *symbol;

    // If already defined, fetch it from the global symbol table.
    symbol = get_global_defined_symbol(name);
    if (symbol) return symbol;

    if (assignment->provide || assignment->provide_hidden) {
        symbol = strmap_ordered_get(provided_symbols, name);
        if (symbol) return symbol;

        symbol = new_symbol(name, STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, 0, 0, 0);
        strmap_ordered_put(provided_symbols, name, symbol);
        if (assignment->provide_hidden) symbol->visibility = STV_HIDDEN;
        symbol->is_abs = 1;
    }
    else {
        symbol = add_defined_symbol(global_symbol_table, name, STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, 0, 0, 0);
        symbol->is_abs = 1;
        resolve_undefined_symbol(name, 0, 0);
    }

    return symbol;
}

// Report an error, or in case we are being tested, write to last_error_message
static void fail(InputElfFile *elf_file, char *format, ...) {
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

static int handle_local_symbol(InputElfFile *elf_file, SymbolTable *local_symbol_table, int is_library, int is_shared_library, int read_only, ElfSymbol *symbol) {
    int strtab_offset = symbol->st_name;
    char *name = &elf_file->symbol_table_strings[symbol->st_name];

    char binding = (symbol->st_info >> 4) & 0xf;
    char type = symbol->st_info & 0xf;
    uint64_t size = symbol->st_size;
    int other = symbol->st_other;

    InputSection *input_section = elf_file->section_list->elements[symbol->st_shndx];

    if (!read_only) {
        Symbol *new_symbol = add_defined_symbol(local_symbol_table, name, type, binding, other, size, is_library, is_shared_library);
        new_symbol->src_elf_file = elf_file;
        new_symbol->input_section = input_section;
        new_symbol->src_value = symbol->st_value;
    }

    return resolve_undefined_symbol(name, is_library, read_only);
}

// Handle a symbol where either the found symbol or symbol is common
// The found symbol may be undefined.
// Returns if the symbol has resolved an undefined symbol
// Not all the cases in https://www.airs.com/blog/archives/49 are handled, just the simple ones.
static int handle_common_symbol(InputElfFile *elf_file, int is_library, int is_shared_library, int read_only, Symbol *found_symbol, ElfSymbol *symbol) {
    int result = 0;

    int strtab_offset = symbol->st_name;
    char *name = &elf_file->symbol_table_strings[symbol->st_name];

    char binding = (symbol->st_info >> 4) & 0xf;
    char type = symbol->st_info & 0xf;
    uint64_t size = symbol->st_size;
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
                InputSection *input_section = elf_file->section_list->elements[symbol->st_shndx];
                found_symbol->src_elf_file = elf_file;
                found_symbol->input_section = input_section;
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
            Symbol *new_symbol = add_defined_symbol(global_symbol_table, name, type, binding, other, size, is_library, is_shared_library);
            new_symbol->src_elf_file = elf_file;
            new_symbol->input_section = NULL;
            new_symbol->src_value = symbol->st_value;
            new_symbol->is_common = 1;
        }

        result = resolve_undefined_symbol(name, is_library, read_only);
    }

    return result;
}

// The new symbol is ABS
// Not very much is checked here, only undefined symbols are resolved.
static int handle_abs_symbol(InputElfFile *elf_file, int is_library, int is_shared_library, int read_only, Symbol *found_symbol, ElfSymbol *symbol) {
    int strtab_offset = symbol->st_name;
    char *name = &elf_file->symbol_table_strings[symbol->st_name];

    char binding = (symbol->st_info >> 4) & 0xf;
    char type = symbol->st_info & 0xf;
    uint64_t size = symbol->st_size;
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
            Symbol *new_symbol = add_defined_symbol(global_symbol_table, name, type, binding, other, size, is_library, is_shared_library);
            new_symbol->src_elf_file = elf_file;
            new_symbol->input_section = NULL;
            new_symbol->src_value = symbol->st_value;
            new_symbol->is_abs = 1;
        }

        return resolve_undefined_symbol(name, is_library, read_only);
    }

    return 0;
}

// Handle a symbol where the left side is defined. Both are not common.
// Returns if the symbol has resolved an undefined symbol
static int handle_non_common_symbol(InputElfFile *elf_file, int is_library,int is_shared_library,  int read_only, Symbol *found_symbol, ElfSymbol *symbol) {
    int result = 0;

    int strtab_offset = symbol->st_name;
    char *name = &elf_file->symbol_table_strings[symbol->st_name];

    char binding = (symbol->st_info >> 4) & 0xf;
    char type = symbol->st_info & 0xf;
    uint64_t size = symbol->st_size;
    int other = symbol->st_other;

    InputSection *input_section = elf_file->section_list->elements[symbol->st_shndx];

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
            // Two cases:
            // - If the new symbol is strong and not in a library, it resolves the exiting weak one.
            // - If the new symbol is strong and is being loaded, it resolves the exiting weak one.
            if (binding != STB_WEAK && (!is_library || !read_only)) {
                // The new symbol is strong and takes over
                result = 1;

                if (!read_only) {
                    found_symbol->src_elf_file = elf_file;
                    found_symbol->input_section = input_section;
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
            Symbol *new_symbol = add_defined_symbol(global_symbol_table, name, type, binding, other, size, is_library, is_shared_library);
            new_symbol->src_elf_file = elf_file;
            new_symbol->input_section = input_section;
            new_symbol->src_value = symbol->st_value;
            new_symbol->is_common = 0;
        }

        result = resolve_undefined_symbol(name, is_library, read_only);
    }

    return result;
}

// Process all symbols in a file. Returns the amount of undefined symbols in
// the symbol table that would be resolved.
int process_elf_file_symbols(InputElfFile *elf_file, int is_library, int is_shared_library, int read_only) {
    last_error_message = NULL;
    int resolved_symbols = 0;

    SymbolTable *local_symbol_table = new_symbol_table();

    if (!read_only) strmap_put(local_symbol_tables, elf_file->filename, local_symbol_table);

    for (int i = 0; i < elf_file->symbol_count; i++) {
        ElfSymbol *symbol = &elf_file->symbol_table[i];

        char binding = (symbol->st_info >> 4) & 0xf;
        char type = symbol->st_info & 0xf;
        uint64_t size = symbol->st_size;
        int other = symbol->st_other;

        if (!symbol->st_name) continue;
        int strtab_offset = symbol->st_name;
        char *name = &elf_file->symbol_table_strings[symbol->st_name];
        int version_index = 0; // TODO

        int is_local = binding == STB_LOCAL;

        if (type == STT_FILE) continue;

        InputSection *input_section = NULL;
        int is_abs = symbol->st_shndx == SHN_ABS;
        int is_common = symbol->st_shndx == SHN_COMMON;
        int is_undef = symbol->st_shndx == SHN_UNDEF;

        // Look up the input_section unless the symbol is common
        if (!is_abs && !is_common) {
            if (symbol->st_shndx >= elf_file->section_list->length)
                fail(elf_file, "Invalid section index in %s: %d >= %d",
                    elf_file->filename, symbol->st_shndx, elf_file->section_list->length);

            input_section = elf_file->section_list->elements[symbol->st_shndx];
        }

        if (is_local) {
            resolved_symbols += handle_local_symbol(elf_file, local_symbol_table, is_library, is_shared_library, read_only, symbol);
        }
        else if (is_undef) {
            // The new symbol is undefined

            Symbol *found_symbol = get_defined_symbol(global_symbol_table, name);
            if (!found_symbol && !read_only) {
                // Add an undefined symbol unless it already exists
                Symbol *undefined_symbol = get_undefined_symbol(name, version_index);

                if (!undefined_symbol) {
                    add_undefined_symbol(name, version_index, type, binding, other, size, is_library);
                }
                else {
                    // Upgrade the undefined symbol from weak to strong
                    if (undefined_symbol->binding == STB_WEAK && binding == STB_GLOBAL)
                        undefined_symbol->binding = STB_GLOBAL;
                }
            }
        }
        else {
            // The new symbol is defined or common

            if (binding != STB_GLOBAL && binding != STB_WEAK)
                panic("Don't know how to handle a symbol with binding %d", binding);

            Symbol *found_symbol = get_defined_symbol(global_symbol_table, name);

            if ((found_symbol && found_symbol->is_common) || is_common)
                resolved_symbols += handle_common_symbol(elf_file, is_library, is_shared_library, read_only, found_symbol, symbol);
            else if ((found_symbol && found_symbol->is_abs) || is_abs)
                resolved_symbols += handle_abs_symbol(elf_file, is_library, is_shared_library, read_only, found_symbol, symbol);
            else
                resolved_symbols += handle_non_common_symbol(elf_file, is_library, is_shared_library, read_only, found_symbol, symbol);
        }
    }

    return resolved_symbols;
}

// Treat all weak symbols as defined, with value zero. Fail if any undefined symbols are left
void finalize_symbols(OutputElfFile *output_elf_file) {
    int count = 0;

    // For all PROVIDE and PROVIDE_HIDDEN symbols, check if there are any undefined symbols that match
    strmap_ordered_foreach(provided_symbols, it) {
        const char *name = strmap_ordered_iterator_key(&it);
        Symbol *provided_symbol = strmap_ordered_get(provided_symbols, name);
        int version_index = 0;
        SymbolNV *snv = new_symbolnv(name, version_index);
        Symbol *symbol = get_undefined_symbol(name, version_index);
        if (symbol) {
            resolve_undefined_symbol(name, 0, 0);
            map_ordered_put(global_symbol_table->defined_symbols, snv, provided_symbol);
        }
    }

    // Make a count of undefined unused unreferenced symbols
    StrMap *global_symbols_in_use = output_elf_file->global_symbols_in_use;
    map_ordered_foreach(global_symbol_table->undefined_symbols, it) {
        const SymbolNV *snv = map_ordered_iterator_key(&it);
        Symbol *symbol = map_ordered_get(global_symbol_table->undefined_symbols, snv);

        // Weak undefined symbols become defined with value zero
        if (symbol->binding == STB_WEAK) {
            symbol->dst_value = 0;
            SymbolNV *new_snv = new_symbolnv(snv->name, snv->version_index);
            map_ordered_put(global_symbol_table->defined_symbols, new_snv, symbol);
        }
        else if (!strmap_get(global_symbols_in_use, snv->name))  {
            if (DEBUG_SYMBOL_RESOLUTION) printf("Ignoring unused undefined symbol %s\n", snv->name);
            continue;
        }
        else
            count++;
    }

    if (!count) return; // All symbols are defined

    printf("Undefined symbols:\n");
    map_ordered_foreach(global_symbol_table->undefined_symbols, it) {
        const SymbolNV *snv = map_ordered_iterator_key(&it);
        Symbol *symbol = map_ordered_get(global_symbol_table->undefined_symbols, snv);
        if (symbol->binding == STB_WEAK) continue;

        printf("  %s\n", snv->name);
    }

    error("Unable to resolve undefined references");
}

static char *print_section_string(Symbol *symbol) {
    OutputSection *rw_section = symbol->output_section;
    int rw_section_index = rw_section ? rw_section->index : 0;

    if (symbol->is_abs)
        printf("ABS");
    else if (symbol->is_common)
        printf("COM");
    else
        printf("%3d", rw_section_index);
}

// Readelf compatible symbol table output
void dump_output_symbols(OutputElfFile *output_elf_file) {
    printf("Symbol Table:\n");
    printf("   Num:     Value         Size Type    Bind   Vis      Ndx Name\n");
    ElfSymbol *symbols = (ElfSymbol *) output_elf_file->section_symtab->data;

    int count = output_elf_file->section_symtab->size / sizeof(ElfSymbol);
    for (int i = 0; i < count; i++) {
        ElfSymbol *symbol = &symbols[i];
        char binding = (symbol->st_info >> 4) & 0xf;
        char type = symbol->st_info & 0xf;
        char visibility = symbol->st_other & 3;
        const char *type_name = SYMBOL_TYPE_NAMES[type];
        const char *binding_name = SYMBOL_BINDING_NAMES[binding];
        const char *visibility_name = SYMBOL_VISIBILITY_NAMES[visibility];

        printf("%6d: %#016lx  %4ld %-8s%-7sDEFAULT  ", i, symbol->st_value, symbol->st_size, type_name, binding_name);
        if ((unsigned short) symbol->st_shndx == SHN_UNDEF)
            printf("UND");
        else if ((unsigned short) symbol->st_shndx == SHN_ABS)
            printf("ABS");
        else if ((unsigned short) symbol->st_shndx == SHN_COMMON)
            printf("COM");
        else
            printf("%3d", symbol->st_shndx);

        int strtab_offset = symbol->st_name;
        if (strtab_offset)
            printf(" %s\n", &output_elf_file->section_strtab->data[strtab_offset]);
        else
            printf("\n");
    }
}

void debug_print_symbol(Symbol *symbol) {
    char binding = symbol->binding;
    char type = symbol->type;
    char visibility = symbol->other & 3;
    const char *type_name = SYMBOL_TYPE_NAMES[type];
    const char *binding_name = SYMBOL_BINDING_NAMES[binding];
    const char *visibility_name = SYMBOL_VISIBILITY_NAMES[visibility];

    printf("%016lx  %4lx %-8s%-7s%-7s  ", symbol->dst_value, symbol->size, type_name, binding_name, visibility_name);
    print_section_string(symbol);
    printf(" %s\n", symbol->name);
}

void debug_summarize_symbols(void) {
    printf("Defined symbols:\n");
    printf("   Num:    Value          Size Type    Bind   Vis        Ndx Name\n");

    int i = 0;
    map_ordered_foreach(global_symbol_table->defined_symbols, it) {
        const SymbolNV *snv = map_ordered_iterator_key(&it);
        Symbol *symbol = map_ordered_get(global_symbol_table->defined_symbols, snv);

        char binding = symbol->binding;
        char type = symbol->type;
        char visibility = symbol->other & 3;
        const char *type_name = SYMBOL_TYPE_NAMES[type];
        const char *binding_name = SYMBOL_BINDING_NAMES[binding];
        const char *visibility_name = SYMBOL_VISIBILITY_NAMES[visibility];

        printf("%6d: %016lx  %4lx %-8s%-7s%-9s  ", i, symbol->dst_value, symbol->size, type_name, binding_name, visibility_name);
        print_section_string(symbol);
        printf(" %s\n", snv->name);

        i++;
    }

    printf("Undefined symbols:\n");
    map_ordered_foreach(global_symbol_table->undefined_symbols, it) {
        const SymbolNV *snv = map_ordered_iterator_key(&it);
        Symbol *symbol = map_ordered_get(global_symbol_table->undefined_symbols, snv);
        if (symbol->binding == STB_WEAK) continue; // It's ok for unresolved symbols to be weak
        debug_print_symbol(symbol);
    }
}

// Returns 1 if any defined symbols are common
int common_symbols_are_present(void) {
    map_ordered_foreach(global_symbol_table->defined_symbols, it) {
        const SymbolNV *snv = map_ordered_iterator_key(&it);
        Symbol *symbol = map_ordered_get(global_symbol_table->defined_symbols, snv);
        if (symbol->is_common) return 1;
    }

    return 0;
}

// Returns 1 if any defined symbols are common
void layout_common_symbols_in_bss_section(OutputSection *bss_section) {
    uint64_t offset = bss_section->size;
    int section_align = 1;

    map_ordered_foreach(global_symbol_table->defined_symbols, it) {
        const SymbolNV *snv = map_ordered_iterator_key(&it);
        Symbol *symbol = map_ordered_get(global_symbol_table->defined_symbols, snv);
        if (!symbol->is_common) continue;

        int symbol_align = symbol->src_value;
        if (symbol_align > section_align) section_align = symbol_align;

        symbol->src_value = offset;
        symbol->output_section = bss_section;

        offset = ALIGN_UP(offset + symbol->size, symbol_align);
    }

    bss_section->align = section_align;
    bss_section->size = offset;
}

// Assign final values to all symbols
void make_symbol_values_from_symbol_table(OutputElfFile *output_elf_file, SymbolTable *symbol_table) {
    map_ordered_foreach(symbol_table->defined_symbols, it) {
        const SymbolNV *snv = map_ordered_iterator_key(&it);
        Symbol *symbol = map_ordered_get(symbol_table->defined_symbols, snv);

        if (symbol->src_is_shared_library) continue;

        if (symbol->is_abs) {
            // Do nothing, it's already resolved
        }
        else if (symbol->is_common) {
            OutputSection *section_bss = output_elf_file->section_bss;
            symbol->dst_value = section_bss->address + symbol->src_value;
            symbol->output_section = section_bss;
        }
        else {
            // Skip weak undefined symbols that don't have a input_section
            if (symbol->input_section) {
                // Get the output section
                if (!symbol->input_section->output_section) panic("Unexpectedly got null output_section for %s from %s\n", symbol->name, symbol->input_section->name);

                OutputSection *rw_section = symbol->input_section->output_section;
                if (!rw_section) panic("Unexpected empty output section for %s", snv->name);
                symbol->output_section = rw_section;

                if (!symbol->input_section)
                    panic("Unexpected null symbol->input_section for %s", snv->name);
                if (!symbol->input_section->output_section)
                    panic("Unexpected null symbol->input_section->output_section for symbol %s in section %s", snv->name, symbol->input_section->name);

                if (symbol->type == STT_TLS)
                    symbol->dst_value = symbol->input_section->output_section->offset + symbol->input_section->dst_offset + symbol->src_value - output_elf_file->tls_template_offset;
                else
                    symbol->dst_value = symbol->input_section->output_section->address + symbol->input_section->dst_offset + symbol->src_value;
            }

            if (DEBUG_RELOCATIONS) {
                printf("%-60s %-60s value=%08lx  ", symbol->name, symbol->src_elf_file ? symbol->src_elf_file->filename : "-", symbol->dst_value);

                if (symbol->binding != STB_WEAK)
                    printf("dst sec off %#0lx sec off %#08lx\n", symbol->input_section->output_section->offset, symbol->input_section->dst_offset);
                else
                    printf("\n");
            }
        }
    }
}

// Add the symbols to the ELF symbol tables.
// The values and will be updated later.
// section indexes are also updated later unless it's an ABS section.
void make_elf_symbols(OutputElfFile *output_elf_file) {
    ElfSectionHeader *elf_section_header = &output_elf_file->elf_section_headers[output_elf_file->section_symtab->index];

    output_elf_file->section_symtab->link = output_elf_file->section_strtab->index;
    output_elf_file->section_symtab->info = output_elf_file->sections_list->length; // Index of the first global symbol

    // .so files don't have a symtab
    if (output_elf_file->type == ET_DYN) return;

    add_elf_symbol(output_elf_file, "", 0, 0, STB_LOCAL, STT_NOTYPE, STV_DEFAULT, SHN_UNDEF); // Null symbol

    for (int i = 1; i < output_elf_file->sections_list->length; i++) {
        OutputSection *section = output_elf_file->sections_list->elements[i];
        add_elf_symbol(output_elf_file, "", 0, 0, STB_LOCAL, STT_SECTION, STV_DEFAULT, i);
    }

    map_ordered_foreach(global_symbol_table->defined_symbols, it) {
        const SymbolNV *snv = map_ordered_iterator_key(&it);
        if (!strcmp(snv->name, ".")) continue;
        Symbol *symbol = map_ordered_get(global_symbol_table->defined_symbols, snv);
        int section_index = symbol->is_abs ? SHN_ABS : SHN_UNDEF;
        symbol->dst_index = add_elf_symbol(output_elf_file, symbol->name, 0, symbol->size, symbol->binding, symbol->type, symbol->visibility, section_index);
    }
}

// Add a string to the dynstr section unless name is "".
// Empty names are all mapped to the first entry in the string table.
int add_dynstr_string(OutputElfFile *output_elf_file, const char *name) {
    InputSection *section_dynstr = output_elf_file->section_dynstr;
    if (!section_dynstr) panic("section_dynstr is NULL");
    int dynstr_offset = *name ? add_to_input_section(section_dynstr, name, strlen(name) + 1) : 0;
    return dynstr_offset;
}

// Add a symbol to the ELF symbol table dynsym
// This function must be called with all local symbols first, then all global symbols
static int add_elf_dyn_symbol(OutputElfFile *output_elf_file, const char *name, long value, long size, int binding, int type, int visibility, int section_index) {
    InputSection *section_dynsym = output_elf_file->section_dynsym;

    int dynstr_offset = add_dynstr_string(output_elf_file, name);

    ElfSymbol *symbol = allocate_in_section(section_dynsym, sizeof(ElfSymbol));
    memset(symbol, 0, sizeof(ElfSymbol));
    symbol->st_name = dynstr_offset;
    symbol->st_value = value;
    symbol->st_size = size;
    symbol->st_info = (binding << 4) + type;
    symbol->st_other = visibility;
    symbol->st_shndx = section_index;

    int index = symbol - (ElfSymbol *) section_dynsym->data;

    return index;
}

// For libraries, add the symbols to the ELF dynsym table
void make_elf_dyn_symbols(OutputElfFile *output_elf_file) {
    if (output_elf_file->type != ET_DYN) return;

    output_elf_file->section_dynstr = create_extra_section(output_elf_file, DYNSTR_SECTION_NAME, SHT_STRTAB, SHF_ALLOC, 1);
    add_to_input_section(output_elf_file->section_dynstr, "", 1);

    output_elf_file->section_dynsym = create_extra_section(output_elf_file, DYNSYM_SECTION_NAME, SHT_DYNSYM, SHF_ALLOC, 1);
    add_elf_dyn_symbol(output_elf_file, "", 0, 0, STB_LOCAL, STT_NOTYPE, STV_DEFAULT, SHN_UNDEF); // Null symbol

    int dynsym_symbol_count = 0;
    int rela_dyn_entry_count = 0;

    map_ordered_foreach(global_symbol_table->defined_symbols, it) {
        const SymbolNV *snv = map_ordered_iterator_key(&it);
        Symbol *symbol = map_ordered_get(global_symbol_table->defined_symbols, snv);

        if (!SYMBOL_IS_IN_DYNSYM(symbol)) continue;

        int section_index = symbol->is_abs ? SHN_ABS : SHN_UNDEF;
        symbol->dst_dynsym_index = add_elf_dyn_symbol(output_elf_file, symbol->name, 0, symbol->size, symbol->binding, symbol->type, symbol->visibility, section_index);

        dynsym_symbol_count++;

        if (symbol->src_is_shared_library && symbol->needs_got) rela_dyn_entry_count++;
    }

    output_elf_file->dynsym_symbol_count = dynsym_symbol_count;
    output_elf_file->rela_dyn_entry_count = rela_dyn_entry_count;
}

// Set the symbol's value and section indexes
void update_elf_symbols(OutputElfFile *output_elf_file) {
    map_ordered_foreach(global_symbol_table->defined_symbols, it) {
        const SymbolNV *snv = map_ordered_iterator_key(&it);
        if (!strcmp(snv->name, ".")) continue;
        Symbol *symbol = map_ordered_get(global_symbol_table->defined_symbols, snv);

        if (!symbol->is_abs && !symbol->output_section) continue; // Weak symbols may not be defined

        if (output_elf_file->type == ET_DYN) {
            // dynsym
            ElfSymbol *elf_symbols = (ElfSymbol *) output_elf_file->section_dynsym->data;
            ElfSymbol *elf_symbol = &elf_symbols[symbol->dst_dynsym_index];
            elf_symbol->st_value = symbol->dst_value;
            elf_symbol->st_shndx = symbol->is_abs ? SHN_ABS : symbol->output_section->index;
        }
        else {
            // symtab
            ElfSymbol *elf_symbols = (ElfSymbol *) output_elf_file->section_symtab->data;
            ElfSymbol *elf_symbol = &elf_symbols[symbol->dst_index];
            elf_symbol->st_value = symbol->dst_value;
            elf_symbol->st_shndx = symbol->is_abs ? SHN_ABS : symbol->output_section->index;
        }
    }
}

static void add_hidden_symbol(char *name) {
    Symbol *symbol = add_defined_symbol(global_symbol_table, name, STT_NOTYPE, STB_GLOBAL, 0, 0, 0, 0);
    symbol->is_abs = 1;
    symbol->visibility = STV_HIDDEN;
}

void init_symbols(OutputElfFile *output_elf_file) {
    global_symbol_table = new_symbol_table();
    local_symbol_tables = new_strmap();
    provided_symbols = new_strmap_ordered();

    if (output_elf_file->type == ET_EXEC)
        add_hidden_symbol(GLOBAL_OFFSET_TABLE_SYMBOL_NAME);
}

// Create the .got, .got.plt, .plt, .rela.plt sections, as needed
void create_got_plt_and_rela_sections(OutputElfFile *output_elf_file) {
    // Scan the symbol table and count the amount of GOT entries
    int got_entries_count = 0;
    int got_plt_entries_count = 0;

    map_ordered_foreach(global_symbol_table->defined_symbols, it) {
        const SymbolNV *snv = map_ordered_iterator_key(&it);
        Symbol *symbol = map_ordered_get(global_symbol_table->defined_symbols, snv);
        if (symbol->needs_got) got_entries_count++;
        if (symbol->needs_got_plt) got_plt_entries_count++;
    }

    output_elf_file->got_plt_entries_count= got_plt_entries_count;

    if (got_entries_count) {
        // Create .got section
        InputSection *extra_section = create_extra_section(output_elf_file, GOT_SECTION_NAME, SHT_PROGBITS, SHF_ALLOC | SHF_WRITE, 8);
        extra_section->size = 8 * got_entries_count;
        extra_section->data = malloc(extra_section->size);
    }

    if (got_plt_entries_count) {
        // Create .got.plt section
        InputSection *extra_section = create_extra_section(output_elf_file, GOT_PLT_SECTION_NAME, SHT_PROGBITS, SHF_ALLOC | SHF_WRITE, 8);
        extra_section->size = 8 * (got_plt_entries_count + 3); // The first three entries of the .got.plt are reserved
        extra_section->data = malloc(extra_section->size);
        output_elf_file->section_got_plt = extra_section;

        // Create .plt section
        extra_section = create_extra_section(output_elf_file, PLT_SECTION_NAME, SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR, 8);
        extra_section->size = 16 * (got_plt_entries_count + 1); // Each entry has 16 bytes. The first entry is reserved.
        extra_section->data = malloc(extra_section->size);

        // Create .rela.plt section
        extra_section = create_extra_section(output_elf_file, RELA_PLT_SECTION_NAME, SHT_RELA, SHF_ALLOC | SHF_INFO_LINK, 8);
        extra_section->size = got_plt_entries_count * sizeof(ElfRelocation);
        extra_section->data = malloc(extra_section->size);
        output_elf_file->section_rela_plt = extra_section;
    }
}

// Set the symbol values in the .got
void update_got_values(OutputElfFile *output_elf_file) {
    InputSection *section_got = get_extra_section(output_elf_file, GOT_SECTION_NAME);
    if (!section_got) return;

    uint64_t got_value = section_got->output_section->address + section_got->dst_offset;

    // Set the special GOT symbol to the virtual address of the GOT
    Symbol *got_symbol = get_global_defined_symbol(GLOBAL_OFFSET_TABLE_SYMBOL_NAME);

    // The got symbol is only defined for statically linked executables
    if (got_symbol) got_symbol->dst_value = got_value;

    // Set the address of the GOT
    output_elf_file->got_virt_address = got_value;

    uint64_t *got_entries = (uint64_t *) section_got->data;

    // Add the .got entries
    int got_index = 0;
    map_ordered_foreach(global_symbol_table->defined_symbols, it) {
        const SymbolNV *snv = map_ordered_iterator_key(&it);
        Symbol *symbol = map_ordered_get(global_symbol_table->defined_symbols, snv);
        if (!symbol->needs_got) continue;

        if (got_index * 8 >= section_got ->size) panic("Trying to write beyond the allocated space in .got");
        got_entries[got_index] = symbol->dst_value;
        symbol->got_offset = got_index * 8;
        got_index++;
    }
}

// Update values in:
// .plt:        This executable section contains pushes and jumps using the addresses in .got.plt
// .got.plt     This contains function addresses. Set here initially and by the program linker during runtime.
// .rela.plt    Relocations in .got.plt
void update_dynamic_relocatable_values(OutputElfFile *output_elf_file) {
    InputSection *section_got_plt = output_elf_file->section_got_plt;
    if (!section_got_plt) return;

    InputSection *section_plt = get_extra_section(output_elf_file, PLT_SECTION_NAME);
    if (!section_plt) panic("section_plt NULL in update_dynamic_relocatable_values");

    InputSection *section_dynamic = get_extra_section(output_elf_file, DYNAMIC_SECTION_NAME);
    if (!section_dynamic) panic("section_dynamic NULL in update_dynamic_relocatable_values");

    InputSection *section_rela_plt = output_elf_file->section_rela_plt;
    if (!section_rela_plt) panic("section_rela_plt NULL in update_dynamic_relocatable_values");

    char *rela_plt_data = section_rela_plt ? section_rela_plt->data : NULL;

    InputSection *section_dynsym = get_extra_section(output_elf_file, DYNSYM_SECTION_NAME);
    if (!section_dynsym) panic("section_dynsym NULL in update_dynamic_relocatable_values");

    uint64_t *got_plt_entries = (uint64_t *) NULL;
    got_plt_entries = (uint64_t *) section_got_plt->data;

    // The first address in .got.plt has to be the address of the .dynamic section
    got_plt_entries[0] =section_dynamic->output_section->offset + section_dynamic->dst_offset;

    // Set plt_offset, used in relocation
    output_elf_file->plt_offset = section_plt->output_section->offset + section_plt->dst_offset;

    // Write the .PLT0 code
    char *plt_data = section_plt->data;
    uint64_t plt_offset = section_plt->output_section->offset + section_plt->dst_offset;
    uint64_t got_plt_offset = section_got_plt->output_section->offset + section_got_plt->dst_offset;

    // pushq .got.plt+8(%rip)
    plt_data[0] = 0xff;
    plt_data[1] = 0x35;
    *((uint32_t *) &plt_data[2]) = got_plt_offset - plt_offset + 8 - 6;

    // jmpq .got.plt+16(%rip)
    plt_data[6] = 0xff;
    plt_data[7] = 0x25;
    *((uint32_t *) &plt_data[8]) = got_plt_offset - plt_offset + 16 - 12;

    // nopl 0x0(%rax)
    plt_data[12] = 0x0f;
    plt_data[13] = 0x1f;
    plt_data[14] = 0x40;

    // Setup the section header for .rela.plt
    // This has to be done in the ELF headers, which have already been made from the output sections.
    section_rela_plt->output_section->link = section_got_plt->index;
    ElfSectionHeader *h = &output_elf_file->elf_section_headers[section_rela_plt->output_section->index];
    h->sh_link = section_dynsym->output_section->index;
    h->sh_info = section_got_plt->output_section->index;
    h->sh_entsize = sizeof(ElfRelocation);

    // Add .plt, .got.plt and .rela.plt entries
    int plt_index = 1; // The first entry has special .plt0 jump code
    int got_plt_index = 3; // The first 3 entries are used by the dynamic linker
    map_ordered_foreach(global_symbol_table->defined_symbols, it) {
        const SymbolNV *snv = map_ordered_iterator_key(&it);
        Symbol *symbol = map_ordered_get(global_symbol_table->defined_symbols, snv);
        if (!symbol->needs_got_plt) continue;

        symbol->plt_offset = plt_index * 16;

        symbol->got_plt_offset = got_plt_index * 8;
        if (symbol->got_plt_offset >= section_got_plt ->size) panic("Trying to write beyond the allocated space in .got.plt");

        // Add instructions to the .plt. The first entry has the .PLT0 stub.
        char *plt_entry_data = section_plt->data + symbol->plt_offset;

        uint64_t plt_offset = section_plt->output_section->offset + section_plt->dst_offset + symbol->plt_offset;
        uint64_t got_plt_offset = section_got_plt->output_section->offset + section_got_plt->dst_offset;

        // Set the .got entry to point to the PLT address + 6, i.e. the address of the pushq instruction.
        // At runtime, the program linker patches this up to point at the actual function.
        got_plt_entries[got_plt_index] = plt_offset + 6;

        // Add instructions to the PLT entry

        int dyn_rela_index = plt_index - 1;

        // jmpq *0x...(%rip)
        // 6 is the size of the instruction, 8 is the offset in the .got.plt table.
        plt_entry_data[0] = 0xff;
        plt_entry_data[1] = 0x25;
        *((uint64_t *) &plt_entry_data[2]) = got_plt_offset + symbol->got_plt_offset - plt_offset - 6;

        // pushq $rela_dyn_index
        plt_entry_data[6] = 0x68;
        *((uint32_t *) &plt_entry_data[7]) = dyn_rela_index;

        // jmpq .plt0
        plt_entry_data[11] = 0xe9;
        *((uint32_t *) &plt_entry_data[12]) = -plt_index * 16 - 16;

        // Add a relocation in .rela.iplt
        if (!rela_plt_data) panic("NULL rela_plt_data in update_got_symbol_values");
        ElfRelocation *r = &((ElfRelocation *) rela_plt_data)[dyn_rela_index];
        r->r_info = R_X86_64_JUMP_SLOT | ((uint64_t) symbol->dst_dynsym_index << 32);
        r->r_offset = got_plt_offset + got_plt_index * 8;
        r->r_addend = 0;

        plt_index++;
        got_plt_index++;
    }
}

// If there are any ifuncs,
// - Create .got.plt
// - Create .iplt
// - Create .rela.iplt
void process_ifuncs_from_symbol_table(OutputElfFile *output_elf_file, SymbolTable *symbol_table) {
    InputSection *section_got_iplt = get_extra_section(output_elf_file, GOT_IPLT_SECTION_NAME);
    InputSection *section_iplt = get_extra_section(output_elf_file, IPLT_SECTION_NAME);
    InputSection *section_rela_iplt = get_extra_section(output_elf_file, RELA_IPLT_SECTION_NAME);

    map_ordered_foreach(symbol_table->defined_symbols, it) {
        const SymbolNV *snv = map_ordered_iterator_key(&it);
        Symbol *symbol = map_ordered_get(symbol_table->defined_symbols, snv);

        if (symbol->type == STT_GNU_IFUNC) {
            // The index in the list corresponds to the index in the sections below.
            append_to_list(output_elf_file->ifunc_symbols, symbol);

            // Reserve .got.iplt entry
            symbol->needs_got_iplt = 1;

            if (!section_got_iplt) {
                section_got_iplt = create_extra_section(output_elf_file, GOT_IPLT_SECTION_NAME, SHT_PROGBITS, SHF_ALLOC | SHF_WRITE, 8);
                section_got_iplt->size = 8;
            }

            symbol->got_iplt_offset = section_got_iplt->size;
            section_got_iplt->size += 8;

            // Reserve .iplt entry
            if (!section_iplt)
                section_iplt = create_extra_section(output_elf_file, IPLT_SECTION_NAME, SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR, 8);
            symbol->iplt_offset = section_iplt->size;
            section_iplt->size += 8; // Enough space for a jmpq *0x...(%rip), padded to 8 bytes

            // Reserve .rela.iplt entry
            if (!section_rela_iplt)
                section_rela_iplt = create_extra_section(output_elf_file, RELA_IPLT_SECTION_NAME, SHT_RELA, SHF_ALLOC | SHF_INFO_LINK, 8);
            symbol->rela_iplt_offset = section_rela_iplt->size;
            section_rela_iplt->size += sizeof(ElfRelocation);
        }
    }
}

// Allocate memory for extra sections like .got.plt and .iplt, .rela.iplt
void allocate_extra_sections(OutputElfFile *output_elf_file) {
    InputSection *section_got_plt = get_extra_section(output_elf_file, GOT_IPLT_SECTION_NAME);
    if (section_got_plt && section_got_plt->size)
        section_got_plt->data = calloc(1, section_got_plt->size);

    InputSection *section_iplt = get_extra_section(output_elf_file, IPLT_SECTION_NAME);
    if (section_iplt && section_iplt->size)
        section_iplt->data = calloc(1, section_iplt->size);

    InputSection *section_rela_iplt = get_extra_section(output_elf_file, RELA_IPLT_SECTION_NAME);
    if (section_rela_iplt && section_rela_iplt->size)
        section_rela_iplt->data = calloc(1, section_rela_iplt->size);
}

// Update addresses and add relocations for ifuncs
// Make .iplt jmp instructions that refer to the entries in .got.iplt
// Note: the calls to the ifunc code aren't rewritten, the loader does that, using the relocations
// added in .rela.iplt.
void update_iplt(OutputElfFile *output_elf_file) {
    InputSection *section_got_plt = get_extra_section(output_elf_file, GOT_IPLT_SECTION_NAME);
    InputSection *section_iplt = get_extra_section(output_elf_file, IPLT_SECTION_NAME);
    InputSection *section_rela_iplt = get_extra_section(output_elf_file, RELA_IPLT_SECTION_NAME);

    if (!section_got_plt && !section_iplt && !section_rela_iplt) return; // No ifuncs are present

    // Check all sections exist
    if (!section_iplt || !section_got_plt)
        panic("Got a %s section without a matching %s section", IPLT_SECTION_NAME, GOT_IPLT_SECTION_NAME);

    if (!section_rela_iplt)
        panic("Got a %s section without a matching %s section", GOT_IPLT_SECTION_NAME, RELA_IPLT_SECTION_NAME);

    // The entries in .plt and .got.plt correspond 1:1. .got.plt has a NULL header and .iplt doesn't.

    if (!section_got_plt->output_section) panic("No address for %s", GOT_IPLT_SECTION_NAME);
    if (!section_iplt->output_section) panic("No address for %s", IPLT_SECTION_NAME);

    uint64_t got_iplt_address = section_got_plt->output_section->address + section_got_plt->dst_offset;
    uint64_t iplt_address = section_iplt->output_section->address + section_iplt->dst_offset;

    char *iplt_data = section_iplt->data;
    char *rela_iplt_data = section_rela_iplt->data;

    // Loop over all ifuncs
    int count = section_iplt->size / 8;
    for (int i = 0; i < count; i++) {
        if (i >= output_elf_file->ifunc_symbols->length) panic("Out of bounds accessing ifunc_symbols");

        Symbol *symbol = (Symbol *) output_elf_file->ifunc_symbols->elements[i];

        // Add an instruction in .iplt
        iplt_data[i * 8 + 0] = 0xff; // jmpq   *0x...(%rip)
        iplt_data[i * 8 + 1] = 0x25;

        // 6 is the size of the instruction, 8 is the offset in the .got.plt table.
        uint64_t value = got_iplt_address - iplt_address + 8 - 6;
        *((uint64_t *) &iplt_data[i * 8 + 2]) = value;

        // Add a relocation in .rela.iplt
        ElfRelocation *r = &((ElfRelocation *) rela_iplt_data)[i];
        r->r_info = R_X86_64_IRELATIVE;
        r->r_offset = got_iplt_address + i * 8 + 8; // .got.plt has one NULL entry
        r->r_addend = symbol->dst_value;
    }

    // Set info in .rela.iplt section to .got.plt section
    // This has to be done in the ELF headers, which have already been made from the output sections.
    section_rela_iplt->output_section->link = section_got_plt->index;
    ElfSectionHeader *h = &output_elf_file->elf_section_headers[section_rela_iplt->output_section->index];
    h->sh_info = section_got_plt->output_section->index;
    h->sh_entsize = sizeof(ElfRelocation);

    // Store the addresses, for use in the relocations code
    output_elf_file->iplt_virt_address = iplt_address;
    output_elf_file->got_iplt_virt_address = got_iplt_address;
}

// ELF hash function
// See https://refspecs.linuxfoundation.org/elf/gabi4+/ch5.dynamic.html#hash
static uint32_t elf_hash(const unsigned char *name) {
    uint32_t h = 0;

    while (*name) {
        h = (h << 4) + *name++;
        uint32_t g = h & 0xf0000000;
        h ^= g >> 24;
        h &= ~g;
    }

    return h;
}

// For ET_DYN files, make a hash table of the symbols
void make_symbol_hashes(OutputElfFile *output_elf_file) {
    if (output_elf_file->type != ET_DYN) return;

    uint64_t dynsym_size = output_elf_file->dynsym_symbol_count + 1; // Include first null entry

    int bucket_count = dynsym_size / 4 + 1;
    int chain_count = dynsym_size;

    // Count symbols
    InputSection *section_hash = output_elf_file->section_hash;
    section_hash->size = 4 * (bucket_count + chain_count + 2);
    section_hash->data = calloc(1, section_hash->size);

    ((uint32_t *) section_hash->data)[0] = bucket_count;
    ((uint32_t *) section_hash->data)[1] = chain_count;

    uint32_t *buckets = section_hash->data + 8;
    uint32_t *chains = buckets + bucket_count;

    int i = 1;
    map_ordered_foreach(global_symbol_table->defined_symbols, it) {
        const SymbolNV *snv = map_ordered_iterator_key(&it);
        Symbol *symbol = map_ordered_get(global_symbol_table->defined_symbols, snv);

        if (!SYMBOL_IS_IN_DYNSYM(symbol)) continue;

        uint32_t hash = elf_hash(snv->name);
        uint32_t bucket = hash % bucket_count;

        if (buckets[bucket] == 0) {
            buckets[bucket] = i;
        }
        else {
            uint32_t index = buckets[bucket];
            while (chains[index] != 0) index = chains[index];
            chains[index] = i;
        }

        i++;
    }
}

// For ET_DYN files, allocate space for relocation entries in the GOT table
void create_dyn_rela_section(OutputElfFile *output_elf_file) {
    if (output_elf_file->type != ET_DYN) return;
    if (output_elf_file->rela_dyn_entry_count == 0) return;

    output_elf_file->section_rela_dyn = create_extra_section(output_elf_file, RELA_DYN_SECTION_NAME, SHT_RELA, SHF_ALLOC, 8);
    output_elf_file->section_rela_dyn->size = output_elf_file->rela_dyn_entry_count * sizeof(ElfRelocation);
    output_elf_file->section_rela_dyn->data = calloc(1, output_elf_file->section_rela_dyn->size);
}

// For ET_DYN files, update the relocation entries in the GOT table
void update_dyn_rela_section(OutputElfFile *output_elf_file) {
    if (output_elf_file->type != ET_DYN) return;
    if (output_elf_file->rela_dyn_entry_count == 0) return;

    InputSection *section_got = get_extra_section(output_elf_file, GOT_SECTION_NAME);
    if (!section_got) panic("GOT is NULL");
    uint64_t got_value = section_got->output_section->address + section_got->dst_offset;

    InputSection *section_rela_dyn = output_elf_file->section_rela_dyn;
    if (!section_rela_dyn) panic(".rela.dyn is NULL");

    // Loop over all global symbols. If they are present in the dynsym table and came from
    // a shared library, then they have entries in the GOT and need entries in .rela.dyn
    // for the dynamic linker to sort out.
    map_ordered_foreach(global_symbol_table->defined_symbols, it) {
        const SymbolNV *snv = map_ordered_iterator_key(&it);
        Symbol *symbol = map_ordered_get(global_symbol_table->defined_symbols, snv);
        if (!SYMBOL_IS_IN_DYNSYM(symbol)) continue;
        if (!symbol->src_is_shared_library) continue;

        if (symbol->needs_got) {
            int i = symbol->got_offset / 8; // The index in the .got table is the same as the index in the .rela.dyn table.
            if (i >= output_elf_file->rela_dyn_entry_count)
                panic("Symbol %s has a GOT offset that exceeds the size of GOT table: %d > %d",
                    symbol->name, i, output_elf_file->rela_dyn_entry_count);

            // Populate the relocation entry in .rela.dyn
            ElfRelocation *r = &((ElfRelocation *) section_rela_dyn->data)[i];
            r->r_info = R_X86_64_GLOB_DAT + ((uint64_t) symbol->dst_dynsym_index << 32);
            r->r_offset = got_value + symbol->got_offset;
            r->r_addend = 0;
        }
     }

    ElfSectionHeader *h = &output_elf_file->elf_section_headers[section_rela_dyn->output_section->index];
    h->sh_link = output_elf_file->section_dynsym->output_section->index;
    h->sh_entsize = sizeof(ElfRelocation);
    h->sh_flags = SHF_ALLOC;
}
