#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "error.h"
#include "input-elf.h"
#include "map-ordered.h"
#include "strmap.h"
#include "strmap-ordered.h"

#include "wld/symbols.h"
#include "wld/script.h"
#include "wld/wld.h"

// Useful for printf, given a symbol name and version index. Use snv->full_name, if available.
#define SYMBOL_VERSION_AND_NAME(name, version_index) \
    name, \
    version_index ? "@" : "", \
    (version_index ? (char *) global_symbol_version_indexes_list->elements[version_index] : "")


typedef struct {
    InputElfFile *elf_file;
    ElfSymbol *elf_symbol;
    SymbolNV *snv; // The symbol's name and value
    char binding;
    char type;
    uint64_t size;
    int other;
    SymbolTable *local_symbol_table;
    int source;    // One of SRC_*
    int read_only;
    int is_default_version;
} ElfSymbolContext;

StrMap *global_symbol_version_indexes_map;
List *global_symbol_version_indexes_list;

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

static int symbol_is_in_dynsym(OutputElfFile *output_elf_file, Symbol *symbol, const SymbolNV *snv) {
    // If we're not making a dynamic ELF, then there is no dynsym
    if (output_elf_file->type != ET_DYN) return 0;

    // Exclude special symbol .
    if (!strcmp(symbol->name, ".")) return 0;

    // Hidden and internal symbols never go in the .dynsym
    if (symbol->other == STV_HIDDEN || symbol->other == STV_INTERNAL) return 0;

    // Local symbols are never go in the .dynsym
    if (symbol->binding == STB_LOCAL) return 0;

    // Don't add defined symbols that aren't exported to .dynsym
    int is_exported_binding = symbol->binding == STB_GLOBAL || symbol->binding == STB_WEAK;
    int is_undefined = is_undefined_symbol(symbol->name, snv->version_index);
    if (!is_exported_binding && !is_undefined) return 0;

    // This symbol is a duplicate of another versioned symbol
    if (snv->is_proxy_for_default) return 0;

    // Include symbols with relocations
    if (symbol->needs_dynsym_entry) return 1;

    // Only include symbols from other shared libraries that resolve undefined symbols.
    if (symbol->sources & SRC_SHARED_LIBRARY) return 0;

    // Implicit else, the symbol is not from a shared library.
    // For executables, include no symbols, since no dynamic linking will be done to them.
    // for shared libraries, include them all, since they need to be exported.
    return !output_elf_file->is_executable;
}

SymbolTable *global_symbol_table;
StrMapOrdered *local_symbol_tables; // Map from filename to symbol table
StrMapOrdered *provided_symbols; // symbols from linker script PROVIDE() and PROVIDE_HIDDEN. Map from symbol name to Symbol

char *last_error_message;

static SymbolNV make_symbolnv(const char *name, int version_index) {
    SymbolNV snv;
    snv.name = name;
    snv.version_index = version_index;

    return snv;
}

static void make_snv_full_name(SymbolNV *snv) {
    if (snv->version_index > GLOBAL_SYMBOL_INDEX_DEFAULT) {
        char *separator = snv->is_default ? "@@" : "@";

        if (snv->version_index >= global_symbol_version_indexes_list->length)
            panic("Symvol version index %d exceeds seen symbol versions %d",
                snv->version_index, global_symbol_version_indexes_list->length);

        char *version_name = global_symbol_version_indexes_list->elements[snv->version_index];
        snv->full_name = malloc(strlen(snv->name) + strlen(separator) + strlen(version_name) + 1);
        sprintf(snv->full_name, "%s%s%s", snv->name, separator, version_name);
    }
    else {
        snv->full_name = strdup(snv->name);
    }
}

SymbolNV *new_symbolnv(const char *name, int version_index, int is_default, int is_proxy_for_default) {
    SymbolNV *snv = malloc(sizeof(SymbolNV));
    snv->name = strdup(name);
    snv->version_index = version_index;
    snv->is_default = is_default;
    snv->is_proxy_for_default = is_proxy_for_default;
    make_snv_full_name(snv);
    return snv;
}

SymbolTable *new_symbol_table(void) {
    SymbolTable *st = malloc(sizeof(SymbolTable));
    st->defined_symbols = new_map_ordered(symbol_nv_hash, symbol_nv_compare);
    st->undefined_symbols = new_map_ordered(symbol_nv_hash, symbol_nv_compare);

    return st;
}

// Get a symbol from the defined symbol table. Returns NULL if not present.
Symbol *get_defined_symbol(SymbolTable *st, const char *name, int version_index) {
    SymbolNV snv = make_symbolnv(name, version_index); // inefficient. No need to make and alloc full_name
    return map_ordered_get(st->defined_symbols, &snv);
}

// Get a symbol from the global defined symbol table. Returns NULL if not present.
Symbol *get_global_defined_symbol(const char *name, int version_index) {
    return get_defined_symbol(global_symbol_table, name, version_index);
}

// Get a symbol from the defined symbol table. Panic if it doesn't exist.
Symbol *must_get_defined_symbol(SymbolTable *st, const char *name, int version_index) {
    Symbol *symbol = get_defined_symbol(st, name, version_index);
    if (!symbol) panic("Symbol not defined %s", name);
    return symbol;
}

// Get a symbol from the global defined symbol table. Panic if it doesn't exist.
Symbol *must_get_global_defined_symbol(const char *name, int version_index) {
    return must_get_defined_symbol(global_symbol_table, name, version_index);
}

// Get an input elf file's local symbol table. Panic if it doesn't exist.
SymbolTable *get_local_symbol_table(InputElfFile *elf_file) {
    SymbolTable *local_symbol_table = strmap_ordered_get(local_symbol_tables, elf_file->filename);
    if (!local_symbol_table) panic("Missing local symbol table for %s", elf_file->filename);
    return local_symbol_table;
}

// Get a symbol from either the local, otherwise the global defined symbol tables. Returns NULL if not present.
Symbol *lookup_symbol(InputElfFile *elf_file, char *name, int version_index) {
    SymbolTable *local_symbol_table = get_local_symbol_table(elf_file);
    Symbol *s = get_defined_symbol(local_symbol_table, name, version_index);
    if (s) return s;
    return get_defined_symbol(global_symbol_table, name, version_index);
}

// Get an undefined symbol. Returns NULL if not present.
Symbol *get_undefined_symbol(const char *name, int version_index) {
    SymbolNV snv = make_symbolnv(name, version_index); // inefficient. No need to make and alloc full_name
    return map_ordered_get(global_symbol_table->undefined_symbols, &snv);
}

// Is a symbol in the undefined symbols set?
int is_undefined_symbol(const char *name, int version_index) {
    return get_undefined_symbol(name, version_index) != NULL;
}

// Remove a symbol from the undefined symbols set
static void remove_undefined_symbol(const char *name, int version_index) {
    if (DEBUG_SYMBOL_RESOLUTION) printf("  Removed undefined %s%s%s\n", SYMBOL_VERSION_AND_NAME(name, version_index));
    SymbolNV snv = make_symbolnv(name, version_index);
    map_ordered_delete(global_symbol_table->undefined_symbols, &snv);
}

Symbol *new_symbol(const char *name, int type, int binding, int other, uint64_t size, int source) {
    Symbol *symbol = calloc(1, sizeof(Symbol));

    symbol->name    = strdup(name);
    symbol->type    = type;
    symbol->binding = binding;
    symbol->other   = other;
    symbol->size    = size;
    symbol->sources = source;

    return symbol;
}

static Symbol *add_defined_symbol(SymbolTable *st, const char *name, int version_index, int type, int binding, int other, uint64_t size, int source) {
    Symbol *symbol = new_symbol(name, type, binding, other, size, source);
    SymbolNV *snv = new_symbolnv(name, version_index, 0, 0);
    map_ordered_put(st->defined_symbols, snv, symbol);

    if (DEBUG_SYMBOL_RESOLUTION)
        printf("  Added defined symbol %s binding=%s other=%s\n", snv->full_name, SYMBOL_BINDING_NAMES[binding], SYMBOL_VISIBILITY_NAMES[other]);


    return symbol;
}

// Check if a symbol resolves an undefined symbol. If so and not read-only the undefined symbol is removed.
// Returns either zero, or a merge of the sources of the resolved symbol, if any.
static int resolve_undefined_symbol(const char *name, int version_index, int is_default_version, int is_library, int read_only) {
    Symbol *undefined_symbol = get_undefined_symbol(name, version_index);

    int undefined_symbol_version_index = version_index;

    // If the symbol hasn't been found and we are resolving a default symbol, try to find an undefined symbol without a version
    if (!undefined_symbol && is_default_version) {
        undefined_symbol = get_undefined_symbol(name, GLOBAL_SYMBOL_INDEX_NONE);
        if (undefined_symbol) undefined_symbol_version_index = 0;
    }

    if (!undefined_symbol) return 0;

    // Resolve the undefined symbol.
    if (!read_only) remove_undefined_symbol(name, undefined_symbol_version_index);

    int sources = undefined_symbol->sources;

    // A defined symbol with a default version may also a second undefined symbol without a version
    if (is_default_version) {
        Symbol *duplicate_undefined_symbol = get_undefined_symbol(name, GLOBAL_SYMBOL_INDEX_NONE);
        if (duplicate_undefined_symbol) {
            if (!read_only) remove_undefined_symbol(name, GLOBAL_SYMBOL_INDEX_NONE);
            sources |= duplicate_undefined_symbol->sources;
        }
    }

    // Weak undefined symbols don't get resolved if also found in a library.
    if (undefined_symbol->binding == STB_WEAK && is_library && read_only) return 0;

    return sources;
}

static Symbol *add_undefined_symbol(const char *name, int version_index, int type, int binding, int other, uint64_t size, int source) {
    Symbol *symbol = new_symbol(name, type, binding, other, size, source);
    SymbolNV *snv = new_symbolnv(name, version_index, 0, 0);
    map_ordered_put(global_symbol_table->undefined_symbols, snv, symbol);

    if (DEBUG_SYMBOL_RESOLUTION) printf("  Added undefined symbol %s\n", snv->full_name);

    return symbol;
}

// Return a defined symbol if it already exists.
// Otherwise, create one.
// Resolve an undefined symbol if there is one.
Symbol *get_or_add_linker_script_symbol(CommandAssignment *assignment) {
    char *name = strdup(assignment->name);

    Symbol *symbol;

    // If already defined, fetch it from the global symbol table.
    symbol = get_global_defined_symbol(name, 0);
    if (symbol) return symbol;

    if (assignment->provide || assignment->provide_hidden) {
        symbol = strmap_ordered_get(provided_symbols, name);
        if (symbol) return symbol;

        symbol = new_symbol(name, STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, 0, SRC_INTERNAL);
        strmap_ordered_put(provided_symbols, name, symbol);
        if (assignment->provide_hidden) symbol->other = STV_HIDDEN;
        symbol->is_abs = 1;
    }
    else {
        int version_index = 0;
        symbol = add_defined_symbol(global_symbol_table, name, version_index, STT_NOTYPE, STB_GLOBAL, 0, 0, SRC_INTERNAL);
        symbol->is_abs = 1;
        resolve_undefined_symbol(name, 0, 0, 0, 0);
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

static int handle_local_symbol(ElfSymbolContext *esc) {
    InputSection *input_section = esc->elf_file->section_list->elements[esc->elf_symbol->st_shndx];

    if (!esc->read_only) {
        Symbol *new_symbol = add_defined_symbol(esc->local_symbol_table, esc->snv->name, esc->snv->version_index, esc->type, esc->binding, esc->other, esc->size, esc->source);
        new_symbol->src_elf_file = esc->elf_file;
        new_symbol->input_section = input_section;
        new_symbol->src_value = esc->elf_symbol->st_value;
    }

    return !!resolve_undefined_symbol(esc->snv->name, esc->snv->version_index, esc->is_default_version, esc->source == SRC_LIBRARY, esc->read_only);
}

// Handle a symbol where either the found symbol or symbol is common
// The found symbol may be undefined.
// Returns if the symbol has resolved an undefined symbol
// Not all the cases in https://www.airs.com/blog/archives/49 are handled, just the simple ones.
static int handle_common_symbol(ElfSymbolContext *esc, Symbol *found_symbol) {
    int result = 0;

    int is_common = esc->elf_symbol->st_shndx == SHN_COMMON;

    if (found_symbol) {
        if (DEBUG_SYMBOL_RESOLUTION)
            printf("  Merging two common symbols %s ...", found_symbol->name);

        if (!found_symbol->is_common && is_common) {
            // Not common - common
            if (DEBUG_SYMBOL_RESOLUTION) printf("doing nothing since existing is not common and new is common\n");
            return 0;
        }
        else if (found_symbol->is_common && !is_common) {
            // Common - not common

            if (!esc->read_only) {

                if (esc->elf_file->type != ET_DYN) {
                    // The new symbol takes over from the common symbol
                    if (DEBUG_SYMBOL_RESOLUTION) printf("the new symbol takes over since it's not common\n");

                    InputSection *input_section = esc->elf_file->section_list->elements[esc->elf_symbol->st_shndx];
                    found_symbol->src_elf_file = esc->elf_file;
                    found_symbol->input_section = input_section;
                    found_symbol->src_value = esc->elf_symbol->st_value;
                    found_symbol->is_common = 0;
                }
                else {
                    // No data is imported from a shared library. It's easiest to use the existing machinery that allocates
                    // .bss entries for common symbols. So ignore the new symbol and let the old one do it's thing.
                    if (DEBUG_SYMBOL_RESOLUTION) printf("the existing common symbol remains, since the new symbol is not common and is in a shared library\n");

                    found_symbol->needs_dynsym_entry = 1;
                }
            }

            return 0;
        }
        else {
            // Common - common
            if (DEBUG_SYMBOL_RESOLUTION) printf("both symbols are common, adjusting size of the existing symbol\n");

            if (esc->elf_symbol->st_size > found_symbol->size) {
                found_symbol->size = esc->elf_symbol->st_size;
                return 1;
            }
        }
    }
    else {
        // The symbol has not yet been defined
        if (!esc->read_only) {
            // Add a new symbol
            Symbol *new_symbol = add_defined_symbol(global_symbol_table, esc->snv->name, esc->snv->version_index, esc->type, esc->binding, esc->other, esc->size, esc->source);
            new_symbol->src_elf_file = esc->elf_file;
            new_symbol->input_section = NULL;
            new_symbol->src_value = esc->elf_symbol->st_value;
            new_symbol->is_common = 1;
        }

        result = !!resolve_undefined_symbol(esc->snv->name, esc->snv->version_index, esc->is_default_version, esc->source == SRC_LIBRARY, esc->read_only);
    }

    return result;
}

// The new symbol is ABS
// Not very much is checked here, only undefined symbols are resolved.
static int handle_abs_symbol(ElfSymbolContext *esc, Symbol *found_symbol) {
    int value = esc->elf_symbol->st_value;

    if (found_symbol) {
        if (found_symbol->src_value != value)
            fail(esc->elf_file, "Conflicting values of absolute symbol %s: %d and %d", esc->snv->name, found_symbol->src_value, value);
    }

    else {
        // The symbol has not yet been defined
        if (!esc->read_only) {
            // Add a new symbol
            Symbol *new_symbol = add_defined_symbol(global_symbol_table, esc->snv->name, esc->snv->version_index, esc->type, esc->binding, esc->other, esc->size, esc->source);
            new_symbol->src_elf_file = esc->elf_file;
            new_symbol->input_section = NULL;
            new_symbol->src_value = esc->elf_symbol->st_value;
            new_symbol->is_abs = 1;
        }

        return !!resolve_undefined_symbol(esc->snv->name, esc->snv->version_index, esc->is_default_version, esc->source == SRC_LIBRARY, esc->read_only);
    }

    return 0;
}

// Handle a symbol where the left side is defined. Both are not common.
// Returns 1 if the symbol has resolved an undefined symbol
static int handle_non_common_symbol(ElfSymbolContext *esc, Symbol *found_symbol) {
    int result = 0;

    InputSection *input_section = esc->elf_file->section_list->elements[esc->elf_symbol->st_shndx];

    if (found_symbol)  {
        if (DEBUG_SYMBOL_RESOLUTION)
            printf("  Merging two symbols %s existing binding=%s found binding=%s ...",
                found_symbol->name, SYMBOL_BINDING_NAMES[found_symbol->binding], SYMBOL_BINDING_NAMES[esc->binding]);

                // Check bindings

        // Two strong bindings
        if (found_symbol->binding != STB_WEAK && esc->binding != STB_WEAK) {
            // Fail with a duplicate symbol in these two cases:
            // obj1 obj2 fail
            // lib1 obj2 fail
            // Libraries don't participate in this check, it's allowed to have duplicate symbols there

            // printf("strong strong %d %d %d - %d %d %d\n", esc->is_object, esc->is_library, esc->is_shared_library, found_symbol->src_is_object, found_symbol->src_is_library, found_symbol->src_is_shared_library);
            if ((found_symbol->sources & SRC_OBJECT_OR_LIBRARY) && esc->source == SRC_OBJECT)
                fail(esc->elf_file, "Multiple definition of %s", esc->snv->full_name);

            // Update the existing symbol sources
            found_symbol->sources |= esc->source;

            // The second symbol is ignored
            if (DEBUG_SYMBOL_RESOLUTION) printf("doing nothing\n");
        }

        // For weak-weak it's first come first served. Use the existing symbol
        else if (found_symbol->binding == STB_WEAK && esc->binding == STB_WEAK) {
            // Do nothing
            if (DEBUG_SYMBOL_RESOLUTION) printf("doing nothing, the existing symbol takes precedence in weak-weak\n");
        }
        else {
            // One is strong and one is weak.
            // Two cases:
            // - If the new symbol is strong and not in a library, it resolves the exiting weak one.
            // - If the new symbol is strong and is being loaded, it resolves the exiting weak one.
            if (esc->binding != STB_WEAK && (!(esc->source & SRC_LIBRARY) || !esc->read_only)) {
                if (DEBUG_SYMBOL_RESOLUTION) printf("the new symbol overrides the existing one\n");
                // The new symbol is strong and takes over
                result = 1;

                if (!esc->read_only) {
                    found_symbol->src_elf_file = esc->elf_file;
                    found_symbol->input_section = input_section;
                    found_symbol->src_value = esc->elf_symbol->st_value;
                    found_symbol->is_common = 0;
                    found_symbol->binding = esc->binding;
                }
            }
            else {
                if (DEBUG_SYMBOL_RESOLUTION) printf("doing nothing, the new weak symbol is ignored\n");
            }

            // Determine if this symbol needs to be in the .dynsym table
            // If the symbol has been seen in an object file or an archive, then it needs to be in the dymsym.
            if (found_symbol->sources & SRC_OBJECT_OR_LIBRARY) found_symbol->needs_dynsym_entry = 1;
        }
    }
    else {
        // The symbol has not yet been defined
        found_symbol = NULL;

        if (!esc->read_only) {
            // Add a new symbol
            found_symbol = add_defined_symbol(global_symbol_table, esc->snv->name, esc->snv->version_index, esc->type, esc->binding, esc->other, esc->size, esc->source);
            found_symbol->src_elf_file = esc->elf_file;
            found_symbol->input_section = input_section;
            found_symbol->src_value = esc->elf_symbol->st_value;
            found_symbol->is_common = 0;
        }
    }

    // Always try to resolve undefined symbols. In particular, there is a case where a weak default symbol might come after a strong non-default symbol.
    // The weak symbol must resolve unversioned undefined symbols.

    // Multiple undefined symbols can have been resolved. We're interested if any of them came from an object file.
    int resolved_undefined_symbol_sources = resolve_undefined_symbol(esc->snv->name, esc->snv->version_index, esc->is_default_version, esc->source == SRC_LIBRARY, esc->read_only);
    result |= !!resolved_undefined_symbol_sources;

    // When loading a symbol from a shared library, note if this symbol resolves an undefined symbol.
    //
    // If:
    // - Symbol is undefined in an object file, so it participates in the link
    // - Symbol type is STT_OBJECT
    // - Symbol is GLOBAL
    // - Symbol is defined in a shared object
    if (
        resolved_undefined_symbol_sources & SRC_OBJECT &&
        found_symbol && found_symbol->type == STT_OBJECT &&
        found_symbol->binding == STB_GLOBAL &&
        esc->source == SRC_SHARED_LIBRARY
    )
        found_symbol->resolves_undefined_symbol = 1;

    return result;
}

void debug_print_global_symbol_version_indexes() {
    printf("Global symbol version indexes:\n");
    for (int i = 0; i < global_symbol_version_indexes_list->length; i++) {
        char *name =  global_symbol_version_indexes_list->elements[i];
        printf("%d %s\n", i, name);
    }
}

// Process all symbols in a file. Returns the amount of undefined symbols in
// the symbol table that would be resolved.
int process_elf_file_symbols(InputElfFile *elf_file, int source, int read_only) {
    last_error_message = NULL;
    int resolved_symbols = 0;

    SymbolTable *local_symbol_table = new_symbol_table();

    if (!read_only) strmap_ordered_put(local_symbol_tables, elf_file->filename, local_symbol_table);

    for (int i = 0; i < elf_file->symbol_count; i++) {
        ElfSymbol *symbol = &elf_file->symbol_table[i];

        char binding = (symbol->st_info >> 4) & 0xf;
        char type = symbol->st_info & 0xf;
        uint64_t size = symbol->st_size;
        int other = symbol->st_other;

        if (type == STT_FILE) continue;

        if (!symbol->st_name) continue;
        char *symbol_name = symbol->st_name ? &elf_file->symbol_table_strings[symbol->st_name] : NULL;
        if (!symbol_name) continue;

        int is_abs = symbol->st_shndx == SHN_ABS;
        int is_common = symbol->st_shndx == SHN_COMMON;
        int is_undef = symbol->st_shndx == SHN_UNDEF;

        int elf_file_local_version_index = elf_file->symbol_table_version_indexes ? elf_file->symbol_table_version_indexes[i] : 0;

        int global_version_index = 0;
        int is_default_version = 0;
        int is_proxy_for_default = 0;

        if (elf_file_local_version_index >= 2) {
            char *version_name = elf_file->symbol_version_names->elements[elf_file_local_version_index];
            int is_non_default_version = (int) (uint64_t) elf_file->non_default_versioned_symbols[i];

            // Set the default from what is set in .gnu.version, unless the symbol is undefined,
            // in which case it is always non-default.
            is_default_version = !is_non_default_version && !is_undef;

            global_version_index = (int) (uint64_t) strmap_get(global_symbol_version_indexes_map, version_name);
            if (!global_version_index) {
                append_to_list(global_symbol_version_indexes_list, version_name);
                global_version_index = global_symbol_version_indexes_list->length - 1;
                strmap_put(global_symbol_version_indexes_map, version_name, (void *) (uint64_t) global_version_index);
            }

            elf_file->global_version_indexes[elf_file_local_version_index] = global_version_index;
        }

        SymbolNV *snv = new_symbolnv(symbol_name, global_version_index, is_default_version, is_proxy_for_default);

        // Create a ElfSymbolContext
        ElfSymbolContext esc;
        esc.elf_file = elf_file;
        esc.elf_symbol = symbol;
        esc.snv = snv;
        esc.binding = binding;
        esc.type = type;
        esc.size = size;
        esc.other = other;
        esc.local_symbol_table = local_symbol_table;
        esc.source = source;
        esc.read_only = read_only;
        esc.is_default_version = is_default_version;

        int is_local = binding == STB_LOCAL;

        InputSection *input_section = NULL;

        if (DEBUG_SYMBOL_VERSIONS && global_version_index > GLOBAL_SYMBOL_INDEX_DEFAULT) {
            char *is_default_string = is_default_version ? " (default)" : "";
            printf("  Found versioned symbol: idx=%d %s%s (%d) is_undef=%d binding=%s is_default_version=%d\n", i, snv->full_name, is_default_string, global_version_index, is_undef, SYMBOL_BINDING_NAMES[binding], is_default_version);
        }


        // Look up the input_section unless the symbol is common
        if (!is_abs && !is_common) {
            if (symbol->st_shndx >= elf_file->section_list->length)
                fail(elf_file, "Invalid section index in %s: %d >= %d",
                    elf_file->filename, symbol->st_shndx, elf_file->section_list->length);

            input_section = elf_file->section_list->elements[symbol->st_shndx];
        }

        if (is_local) {
            resolved_symbols += handle_local_symbol(&esc);
        }
        else if (is_undef) {
            // The new symbol is undefined

            Symbol *found_symbol = get_defined_symbol(global_symbol_table, snv->name, snv->version_index);
            if (!found_symbol && !read_only) {
                // Add an undefined symbol unless it already exists
                Symbol *undefined_symbol = get_undefined_symbol(snv->name, snv->version_index);

                if (!undefined_symbol) {
                    add_undefined_symbol(snv->name, snv->version_index, type, binding, other, size, source);
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

            Symbol *found_symbol = get_defined_symbol(global_symbol_table, snv->name, snv->version_index);

            if ((found_symbol && found_symbol->is_common) || is_common)
                resolved_symbols += handle_common_symbol(&esc, found_symbol);
            else if ((found_symbol && found_symbol->is_abs) || is_abs)
                resolved_symbols += handle_abs_symbol(&esc, found_symbol);
            else
                resolved_symbols += handle_non_common_symbol(&esc, found_symbol);
        }

        if (is_default_version) {
            // Add an entry to the symbol table GLOBAL_SYMBOL_INDEX_NONE, so that unversioned symbols
            // resolve to the default.
            Symbol *symbol = must_get_defined_symbol(global_symbol_table, symbol_name, global_version_index);
            SymbolNV *snv = new_symbolnv(symbol_name, GLOBAL_SYMBOL_INDEX_NONE, 0, 1);
            map_ordered_put(global_symbol_table->defined_symbols, snv, symbol);
        }
    }

    return resolved_symbols;
}

// For all PROVIDE and PROVIDE_HIDDEN symbols, check if there are any undefined symbols that match
void resolve_provided_symbols(OutputElfFile *output_elf_file) {
    strmap_ordered_foreach(provided_symbols, it) {
        const char *name = strmap_ordered_iterator_key(&it);
        Symbol *provided_symbol = strmap_ordered_get(provided_symbols, name);
        int version_index = 0;
        SymbolNV *snv = new_symbolnv(name, version_index, 0, 0);
        Symbol *symbol = get_undefined_symbol(name, version_index);
        if (symbol) {
            resolve_undefined_symbol(name, 0, 0, 0, 0);
            map_ordered_put(global_symbol_table->defined_symbols, snv, provided_symbol);
        }
    }
}

// Treat all weak symbols as defined, with value zero. Fail if any undefined symbols are left
void finalize_symbols(OutputElfFile *output_elf_file) {
    List *undefined_symbol_names = new_list(32);

    // Make a count of undefined unused unreferenced symbols
    StrMap *global_symbols_in_use = output_elf_file->global_symbols_in_use;
    map_ordered_foreach(global_symbol_table->undefined_symbols, it) {
        const SymbolNV *snv = map_ordered_iterator_key(&it);
        Symbol *symbol = map_ordered_get(global_symbol_table->undefined_symbols, snv);
        symbol->is_undefined = 1;

        // Don't resovle a _DYNAMIC WEAK symbol. It may be included through a .o crt file
        // but is only needed for an ET_DYN ELF file.
        if (!strcmp(snv->name, DYNAMIC_SYMBOL_NAME)) continue;

        // If the symbol has been found in a shared library then it's allowed to be undefined.
        if (symbol->sources & SRC_SHARED_LIBRARY)  {
            if (DEBUG_SYMBOL_RESOLUTION) printf("Ignoring undefined %ss in lib\n" , snv->full_name);
            continue;
        }
        // Weak undefined symbols become defined with value zero
        else if (symbol->binding == STB_WEAK) {
            symbol->dst_value = 0;
            SymbolNV *new_snv = new_symbolnv(snv->name, snv->version_index, 0, 0);
            map_ordered_put(global_symbol_table->defined_symbols, new_snv, symbol);
        }
        else if (!strmap_get(global_symbols_in_use, snv->name)) {
            // Note: this doesn't take symbol versions into account.
            if (DEBUG_SYMBOL_RESOLUTION) printf("Ignoring unused undefined symbol %s\n", snv->full_name);
            continue;
        }
        else {
            append_to_list(undefined_symbol_names, (char *) snv->full_name);
        }
    }

    if (!undefined_symbol_names->length) {
        free_list(undefined_symbol_names);
        return; // All symbols are defined
    }

    fprintf(stderr, "Undefined symbols:\n");
    for (int i = 0; i < undefined_symbol_names->length; i++) {
        char *name = undefined_symbol_names->elements[i];
        fprintf(stderr, "  %s\n", name);
    }

    error("Unable to resolve undefined references");
}

// When making a shared library, convert all weak symbols to local, with default visibility
void convert_hidden_symbols(OutputElfFile *output_elf_file) {
    if (output_elf_file->type != ET_DYN || output_elf_file->is_executable) return;

    map_ordered_foreach(global_symbol_table->defined_symbols, it) {
        const SymbolNV *snv = map_ordered_iterator_key(&it);
        Symbol *symbol = map_ordered_get(global_symbol_table->defined_symbols, snv);
        if (symbol->other != STV_HIDDEN) continue;
        if (DEBUG_SYMBOL_RESOLUTION) printf("Converting %s from hidden to local/default\n", snv->full_name);
        symbol->binding = STB_LOCAL;
        symbol->other = STV_DEFAULT;
    }
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

static void print_symbol_table_symbols(SymbolTable *symbol_table, const char *filename) {
    int printed_filename = 0;
    int i = 0;
    map_ordered_foreach(symbol_table->defined_symbols, it) {
        if (!printed_filename && filename) {
            printf("\n%s:\n", filename);
            printed_filename = 1;
        }

        const SymbolNV *snv = map_ordered_iterator_key(&it);
        Symbol *symbol = map_ordered_get(symbol_table->defined_symbols, snv);

        char binding = symbol->binding;
        char type = symbol->type;
        char visibility = symbol->other & 3;
        const char *type_name = SYMBOL_TYPE_NAMES[type];
        const char *binding_name = SYMBOL_BINDING_NAMES[binding];
        const char *visibility_name = SYMBOL_VISIBILITY_NAMES[visibility];

        printf("%6d: %016lx  %4lx %-8s%-7s%-9s",
            i, symbol->dst_value, symbol->size, type_name, binding_name, visibility_name);
        print_section_string(symbol);
        printf(" %s\n", snv->full_name);

        i++;
    }
}

void debug_summarize_symbols() {
    printf("Defined symbols:\n");
    printf("   Num:    Value          Size Type    Bind   Vis        Ndx Name\n");

    print_symbol_table_symbols(global_symbol_table, NULL);

    strmap_ordered_foreach(local_symbol_tables, it) {
        const char *filename = strmap_ordered_iterator_key(&it);
        SymbolTable *local_symbol_table = strmap_ordered_get(local_symbol_tables, filename);
        print_symbol_table_symbols(local_symbol_table, filename);
    }

    printf("\nUndefined symbols:\n");
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

        // Symbol values can only be made when the source file came from an object file or library
        if (!(symbol->sources & SRC_OBJECT_OR_LIBRARY)) continue;

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
                if (!symbol->input_section->output_section) panic("Unexpectedly got null output_section for %s from %s\n",
                    symbol->name, symbol->input_section->name);

                OutputSection *rw_section = symbol->input_section->output_section;
                if (!rw_section) panic("Unexpected empty output section for %s", snv->full_name);
                symbol->output_section = rw_section;

                if (!symbol->input_section)
                    panic("Unexpected null symbol->input_section for %s", snv->full_name);
                if (!symbol->input_section->output_section)
                    panic("Unexpected null symbol->input_section->output_section for symbol %s in section %s", snv->full_name, symbol->input_section->name);

                if (symbol->type == STT_TLS)
                    symbol->dst_value = symbol->input_section->output_section->offset + symbol->input_section->dst_offset + symbol->src_value - output_elf_file->tls_template_offset;
                else
                    symbol->dst_value = symbol->input_section->output_section->address + symbol->input_section->dst_offset + symbol->src_value;
            }

            if (DEBUG_RELOCATIONS) {
                printf("%-60s %-60s value=%08lx  ", snv->full_name, symbol->src_elf_file ? symbol->src_elf_file->filename : "-", symbol->dst_value);

                if (symbol->binding != STB_WEAK) {
                    if (symbol->input_section)
                        printf("dst sec off %#0lx sec off %#08lx\n", symbol->input_section->output_section->offset, symbol->input_section->dst_offset);
                    else
                        printf("no input section");
                }
                else
                    printf("\n");
            }
        }
    }
}

// Add a local or global symbol to the ELF file
static void add_elf_symbols_from_symbol_table (OutputElfFile *output_elf_file, SymbolTable *symbol_table, int binding) {
    map_ordered_foreach(symbol_table->defined_symbols, it) {
        const SymbolNV *snv = map_ordered_iterator_key(&it);
        if (!strcmp(snv->name, ".")) continue;
        Symbol *symbol = map_ordered_get(symbol_table->defined_symbols, snv);
        if (symbol->binding != binding) continue;

        // Don't add a symbol if it has only been seen in a shared library
        if (output_elf_file->type == ET_DYN && !(symbol->sources & (SRC_INTERNAL | SRC_OBJECT | SRC_LIBRARY))) continue;

        int section_index = symbol->is_abs ? SHN_ABS : SHN_UNDEF;
        symbol->dst_index = add_elf_symbol(output_elf_file, symbol->name, 0, symbol->size, symbol->binding, symbol->type, symbol->other, section_index);

        // Update the symtab info section so that it points to the last local symbol
        if (binding == STB_LOCAL) output_elf_file->section_symtab->info = symbol->dst_index + 1;
    }
}

// Add the symbols to the ELF symbol tables.
// The values and will be updated later.
// section indexes are also updated later unless it's an ABS section.
void make_elf_symbols(OutputElfFile *output_elf_file) {
    ElfSectionHeader *elf_section_header = &output_elf_file->elf_section_headers[output_elf_file->section_symtab->index];

    output_elf_file->section_symtab->link = output_elf_file->section_strtab->index;

    // Add null symbol
    add_elf_symbol(output_elf_file, "", 0, 0, STB_LOCAL, STT_NOTYPE, STV_DEFAULT, SHN_UNDEF);

    // Add section symbols
    for (int i = 1; i < output_elf_file->sections_list->length; i++) {
        OutputSection *section = output_elf_file->sections_list->elements[i];
        int dst_index = add_elf_symbol(output_elf_file, "", 0, 0, STB_LOCAL, STT_SECTION, STV_DEFAULT, i);
        output_elf_file->section_symtab->info = dst_index + 1;
    }

    // Add global "local" builtin special symbols
    add_elf_symbols_from_symbol_table(output_elf_file, global_symbol_table, STB_LOCAL);

    // Add local symbols
    strmap_ordered_foreach(local_symbol_tables, it) {
        const char *filename = strmap_ordered_iterator_key(&it);
        SymbolTable *local_symbol_table = strmap_ordered_get(local_symbol_tables, filename);
        add_elf_symbols_from_symbol_table(output_elf_file, local_symbol_table, STB_LOCAL);
    }

    // Add global symbols
    add_elf_symbols_from_symbol_table(output_elf_file, global_symbol_table, STB_GLOBAL);

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
static int add_elf_dyn_symbol(OutputElfFile *output_elf_file, const char *name, long value, long size, int binding, int type, int other, int section_index) {
    InputSection *section_dynsym = output_elf_file->section_dynsym;

    int dynstr_offset = add_dynstr_string(output_elf_file, name);

    ElfSymbol *symbol = allocate_in_section(section_dynsym, sizeof(ElfSymbol));
    memset(symbol, 0, sizeof(ElfSymbol));
    symbol->st_name = dynstr_offset;
    symbol->st_value = value;
    symbol->st_size = size;
    symbol->st_info = (binding << 4) + type;
    symbol->st_other = other;
    symbol->st_shndx = section_index;

    int index = symbol - (ElfSymbol *) section_dynsym->data;

    return index;
}

// For libraries, add the symbols to the ELF dynsym table
void make_elf_dyn_symbols(OutputElfFile *output_elf_file) {
    if (output_elf_file->type != ET_DYN) return;

    int *seen_version_indexes = calloc(1, global_symbol_version_indexes_list->length * sizeof(int));

    output_elf_file->section_dynstr = create_extra_section(output_elf_file, DYNSTR_SECTION_NAME, SHT_STRTAB, SHF_ALLOC, 1);
    add_to_input_section(output_elf_file->section_dynstr, "", 1);

    output_elf_file->section_dynsym = create_extra_section(output_elf_file, DYNSYM_SECTION_NAME, SHT_DYNSYM, SHF_ALLOC, 1);
    add_elf_dyn_symbol(output_elf_file, "", 0, 0, STB_LOCAL, STT_NOTYPE, STV_DEFAULT, SHN_UNDEF); // Null symbol

    int dynsym_symbol_count = 0;
    int rela_dyn_entry_count = 0;

    // Process global symbols
    map_ordered_foreach(global_symbol_table->defined_symbols, it) {
        const SymbolNV *snv = map_ordered_iterator_key(&it);
        Symbol *symbol = map_ordered_get(global_symbol_table->defined_symbols, snv);

        if (!symbol_is_in_dynsym(output_elf_file, symbol, snv)) continue;

        seen_version_indexes[snv->version_index] = 1;

        int section_index = symbol->is_abs ? SHN_ABS : SHN_UNDEF;
        symbol->dst_dynsym_index = add_elf_dyn_symbol(output_elf_file, symbol->name, 0, symbol->size, symbol->binding, symbol->type, symbol->other, section_index);

        dynsym_symbol_count++;

        if (symbol->extra & SE_IN_GOT) rela_dyn_entry_count++;
        if (symbol->extra & SE_COPY_RELOCATION) rela_dyn_entry_count++;
    }

    // Process local symbols. There are no .dynsym entries for them, but they can be in the .rela.dyn table
    strmap_ordered_foreach(local_symbol_tables, it) {
        const char *filename = strmap_ordered_iterator_key(&it);
        SymbolTable *local_symbol_table = strmap_ordered_get(local_symbol_tables, filename);
        map_ordered_foreach(local_symbol_table->defined_symbols, it) {
            const SymbolNV *snv = map_ordered_iterator_key(&it);
            Symbol *symbol = map_ordered_get(local_symbol_table->defined_symbols, snv);
            seen_version_indexes[snv->version_index] = 1;
            if (symbol->extra & SE_IN_GOT) rela_dyn_entry_count++;
            if (symbol->extra & SE_COPY_RELOCATION) rela_dyn_entry_count++;
        }
    }

    rela_dyn_entry_count += output_elf_file->rela_dyn_R_X86_64_RELATIVE_relocations->length;
    rela_dyn_entry_count += output_elf_file->rela_dyn_R_X86_64_64_relocations->length;

    output_elf_file->dynsym_symbol_count = dynsym_symbol_count;
    output_elf_file->rela_dyn_entry_count = rela_dyn_entry_count;

    // Make verneed_names and mapping from global version index to output version index
    output_elf_file->verneed_names = new_list(global_symbol_version_indexes_list->length);
    output_elf_file->verneed_indexes = calloc(1, global_symbol_version_indexes_list->length * sizeof(int));

    if (DEBUG_SYMBOL_VERSIONS) printf("\nMaking mapping from global to output version index\n");
    for (int i = 2; i < global_symbol_version_indexes_list->length; i++) {
        char *version_name = global_symbol_version_indexes_list->elements[i];
        if (!seen_version_indexes[i]) continue;
        int output_version_index = output_elf_file->verneed_names->length + 2;
        append_to_list(output_elf_file->verneed_names, version_name);
        output_elf_file->verneed_indexes[i] = output_version_index;
        if (DEBUG_SYMBOL_VERSIONS) printf("%3d -> %3d for %s \n", i, output_version_index, version_name);
    }

    free(seen_version_indexes);
}

// Set the symbol's value and section indexes
void update_elf_symbols(OutputElfFile *output_elf_file) {
    // Global symbols
    map_ordered_foreach(global_symbol_table->defined_symbols, it) {
        const SymbolNV *snv = map_ordered_iterator_key(&it);
        if (!strcmp(snv->name, ".")) continue;
        Symbol *symbol = map_ordered_get(global_symbol_table->defined_symbols, snv);

        if (output_elf_file->type == ET_DYN && !strcmp(symbol->name, DYNAMIC_SYMBOL_NAME)) {
            ElfSymbol *elf_symbols = (ElfSymbol *) output_elf_file->section_symtab->data;
            ElfSymbol *elf_symbol = &elf_symbols[symbol->dst_index];

            elf_symbol->st_value = symbol->dst_value;

            OutputSection *dynamic_section = get_output_section(output_elf_file, DYNAMIC_SECTION_NAME);
            if (!dynamic_section) panic("%s section is unexpectedly missing", DYNAMIC_SECTION_NAME);

            elf_symbol->st_value = dynamic_section->offset;
            elf_symbol->st_shndx = dynamic_section->index;
        }

        if (!strcmp(symbol->name, GLOBAL_OFFSET_TABLE_SYMBOL_NAME)) {
            ElfSymbol *elf_symbols = (ElfSymbol *) output_elf_file->section_symtab->data;
            ElfSymbol *elf_symbol = &elf_symbols[symbol->dst_index];

            if (output_elf_file->got_entries_count) {
                OutputSection *section = get_output_section(output_elf_file, GOT_SECTION_NAME);
                elf_symbol->st_value = symbol->dst_value;
                elf_symbol->st_shndx = section->index;
            }

            if (output_elf_file->got_plt_entries_count) {
                OutputSection *section = get_output_section(output_elf_file, GOT_PLT_SECTION_NAME);
                elf_symbol->st_value = symbol->dst_value;
                elf_symbol->st_shndx = section->index;
            }
        }

        if (!symbol->is_abs && !symbol->output_section) continue; // Weak symbols may not be defined

        // dynsym
        if (output_elf_file->type == ET_DYN) {
            ElfSymbol *elf_symbols = (ElfSymbol *) output_elf_file->section_dynsym->data;
            ElfSymbol *elf_symbol = &elf_symbols[symbol->dst_dynsym_index];

            elf_symbol->st_value = symbol->dst_value;
            elf_symbol->st_shndx = symbol->is_abs ? SHN_ABS : symbol->output_section->index;
        }

        // symtab
        if (symbol->dst_index) {
            ElfSymbol *elf_symbols = (ElfSymbol *) output_elf_file->section_symtab->data;
            ElfSymbol *elf_symbol = &elf_symbols[symbol->dst_index];
            elf_symbol->st_value = symbol->dst_value;
            elf_symbol->st_shndx = symbol->is_abs ? SHN_ABS : symbol->output_section->index;
        }
    }

    // Local symbols
    strmap_ordered_foreach(local_symbol_tables, it) {
        const char *filename = strmap_ordered_iterator_key(&it);
        SymbolTable *local_symbol_table = strmap_ordered_get(local_symbol_tables, filename);
        map_ordered_foreach(local_symbol_table->defined_symbols, it) {
            const SymbolNV *snv = map_ordered_iterator_key(&it);
            Symbol *symbol = map_ordered_get(local_symbol_table->defined_symbols, snv);

            if (symbol->dst_index) {
                ElfSymbol *elf_symbols = (ElfSymbol *) output_elf_file->section_symtab->data;
                ElfSymbol *elf_symbol = &elf_symbols[symbol->dst_index];
                elf_symbol->st_value = symbol->dst_value;
                elf_symbol->st_shndx = symbol->is_abs ? SHN_ABS : symbol->output_section->index;
            }
        }
    }
}

static Symbol *add_hidden_symbol(char *name) {
    Symbol *symbol = add_defined_symbol(global_symbol_table, name, 0, STT_NOTYPE, STB_GLOBAL, 0, 0, SRC_INTERNAL);
    symbol->is_abs = 1;
    symbol->other = STV_HIDDEN;
    return symbol;
}

void init_symbols(OutputElfFile *output_elf_file) {
    global_symbol_table = new_symbol_table();
    local_symbol_tables = new_strmap_ordered();
    provided_symbols = new_strmap_ordered();
    global_symbol_version_indexes_map = new_strmap();
    global_symbol_version_indexes_list = new_list(32);
    append_to_list(global_symbol_version_indexes_list, NULL); // Skip first element, the zeroth element means not versioned
    append_to_list(global_symbol_version_indexes_list, "__DEFAULT"); // Add default symbol version
}

// Create the .got, .got.plt, .plt, .rela.plt sections, as needed
void create_got_plt_and_rela_sections(OutputElfFile *output_elf_file) {
    // Scan the symbol table and count the amount of GOT entries
    int got_entries_count = 0;
    int got_plt_entries_count = 0;

    // Process global symbols
    map_ordered_foreach(global_symbol_table->defined_symbols, it) {
        const SymbolNV *snv = map_ordered_iterator_key(&it);
        Symbol *symbol = map_ordered_get(global_symbol_table->defined_symbols, snv);
        if (symbol->extra & SE_IN_GOT) got_entries_count++;
        if (symbol->extra & SE_IN_GOT_PLT) got_plt_entries_count++;
    }

    // Process local symbols
    strmap_ordered_foreach(local_symbol_tables, it) {
        const char *filename = strmap_ordered_iterator_key(&it);
        SymbolTable *local_symbol_table = strmap_ordered_get(local_symbol_tables, filename);
        map_ordered_foreach(local_symbol_table->defined_symbols, it) {
            const SymbolNV *snv = map_ordered_iterator_key(&it);
            Symbol *symbol = map_ordered_get(local_symbol_table->defined_symbols, snv);
            if (symbol->extra & SE_IN_GOT) got_entries_count++;
                if (symbol->extra & SE_IN_GOT_PLT) got_plt_entries_count++;
        }
    }

    output_elf_file->got_entries_count = got_entries_count;
    output_elf_file->got_plt_entries_count = got_plt_entries_count;

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

// Create the _GLOBAL_OFFSET_TABLE_ symbol if it's needed
void create_got_symbol(OutputElfFile *output_elf_file) {
    if (output_elf_file->got_entries_count + output_elf_file->got_plt_entries_count > 0)
        add_defined_symbol(global_symbol_table, GLOBAL_OFFSET_TABLE_SYMBOL_NAME, 0, STT_OBJECT, STB_LOCAL, 0, 0, SRC_INTERNAL);
}

static void update_got_values_from_symbol_table(InputSection *section_got, SymbolTable *symbol_table, int *pgot_index) {
    uint64_t *got_entries = (uint64_t *) section_got->data;

    map_ordered_foreach(symbol_table->defined_symbols, it) {
        const SymbolNV *snv = map_ordered_iterator_key(&it);
        Symbol *symbol = map_ordered_get(symbol_table->defined_symbols, snv);
        if (!(symbol->extra & SE_IN_GOT)) continue;

        if (*pgot_index * 8 >= section_got ->size) panic("Trying to write beyond the allocated space in .got");
        got_entries[*pgot_index] = symbol->dst_value;
        symbol->got_offset = *pgot_index * 8;
        (*pgot_index)++;
    }
}

// Set the symbol values in the .got
void update_got_values(OutputElfFile *output_elf_file) {
    InputSection *section_got = get_extra_section(output_elf_file, GOT_SECTION_NAME);
    if (!section_got) return;

    uint64_t got_value = section_got->output_section->address + section_got->dst_offset;

    // Set the address of the GOT
    output_elf_file->got_virt_address = got_value;

    uint64_t *got_entries = (uint64_t *) section_got->data;

    // Add the .got entries
    int got_index = 0;

    // Process global symbols
    update_got_values_from_symbol_table(section_got, global_symbol_table, &got_index);

    // Process local symbols
    strmap_ordered_foreach(local_symbol_tables, it) {
        const char *filename = strmap_ordered_iterator_key(&it);
        SymbolTable *local_symbol_table = strmap_ordered_get(local_symbol_tables, filename);
        update_got_values_from_symbol_table(section_got, local_symbol_table, &got_index);
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
    got_plt_entries[0] = section_dynamic->output_section->offset + section_dynamic->dst_offset;
    got_plt_entries[1] = 0; // Required to be zero by the dynamic linker
    got_plt_entries[2] = 0; // Required to be zero by the dynamic linker

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
        if (!(symbol->extra & SE_IN_GOT_PLT)) continue;

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

// Set the _GLOBAL_OFFSET_TABLE_ symbol to the address of the .got or .got.plt section
void set_got_symbol_value(OutputElfFile *output_elf_file) {
    InputSection *section_got = get_extra_section(output_elf_file, GOT_SECTION_NAME);
    if (section_got) {
        uint64_t got_value = section_got->output_section->address + section_got->dst_offset;
        Symbol *got_symbol = get_global_defined_symbol(GLOBAL_OFFSET_TABLE_SYMBOL_NAME, 0);
        if (got_symbol) got_symbol->dst_value = got_value;
    }

    InputSection *section_got_plt = get_extra_section(output_elf_file, GOT_PLT_SECTION_NAME);
    if (section_got_plt) {
        uint64_t got_value = section_got_plt->output_section->address + section_got_plt->dst_offset;
        Symbol *got_symbol = get_global_defined_symbol(GLOBAL_OFFSET_TABLE_SYMBOL_NAME, 0);
        if (got_symbol) got_symbol->dst_value = got_value;
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
            symbol->extra |= SE_IN_GOT_IPLT;
            symbol->needs_dynsym_entry = 1;

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

        if (!symbol_is_in_dynsym(output_elf_file, symbol, snv)) continue;

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

// For ET_DYN files, update the relocation entries in the .rela.dyn table
// This includes
// - Entries in the .got
// - Symbols that have a R_X86_64_COPY relocation
//
// The first part of the table are all .got entries, with a 1:1 mapping with .got.
// The second have are R_X86_64_COPY relocations.
void update_dyn_rela_section(OutputElfFile *output_elf_file) {
    if (output_elf_file->type != ET_DYN) return;
    if (output_elf_file->rela_dyn_entry_count == 0) return;

    InputSection *section_got = get_extra_section(output_elf_file, GOT_SECTION_NAME);
    uint64_t got_value = section_got ? section_got->output_section->address + section_got->dst_offset : 0;

    InputSection *section_rela_dyn = output_elf_file->section_rela_dyn;
    if (!section_rela_dyn) panic(".rela.dyn is NULL");

    int highest_got_entry = -1;

    // Loop over all global symbols. If they are present in the dynsym table and came from
    // a shared library, then they have entries in the GOT and need entries in .rela.dyn
    // for the dynamic linker to sort out.
    int i = 0;
    map_ordered_foreach(global_symbol_table->defined_symbols, it) {
        const SymbolNV *snv = map_ordered_iterator_key(&it);
        Symbol *symbol = map_ordered_get(global_symbol_table->defined_symbols, snv);
        if (!symbol_is_in_dynsym(output_elf_file, symbol, snv)) continue;

        if (!(symbol->extra & SE_IN_GOT)) continue;
        if (!section_got) panic("GOT is NULL");

        if (i >= output_elf_file->rela_dyn_entry_count)
            panic("Symbol %s has a GOT offset that exceeds the size of .rela.dyn table: %d > %d",
                symbol->name, i, output_elf_file->rela_dyn_entry_count);

        if (i > highest_got_entry) highest_got_entry = i;

        // Populate the relocation entry in .rela.dyn
        if (i >= section_rela_dyn->size) panic("Attempt to write beyond .rela.dyn at %d, size=%d", i, section_rela_dyn->size);
        ElfRelocation *r = &((ElfRelocation *) section_rela_dyn->data)[i];
        r->r_info = R_X86_64_GLOB_DAT + ((uint64_t) symbol->dst_dynsym_index << 32);
        r->r_offset = got_value + symbol->got_offset;
        r->r_addend = 0;

        i++;
    }

    // Similar to above, loop over local symbols that need a GOT entry.
    // The symbol not in the .dynsym since it's local.
    // A R_X86_64_RELATIVE (B + A) relocation is added using an offset instead of a symbol.
    strmap_ordered_foreach(local_symbol_tables, it) {
        const char *filename = strmap_ordered_iterator_key(&it);
        SymbolTable *local_symbol_table = strmap_ordered_get(local_symbol_tables, filename);
        map_ordered_foreach(local_symbol_table->defined_symbols, it) {
            const SymbolNV *snv = map_ordered_iterator_key(&it);
            Symbol *symbol = map_ordered_get(local_symbol_table->defined_symbols, snv);
            if (!(symbol->extra & SE_IN_GOT)) continue;
            if (!section_got) panic("GOT is NULL");

            int i = symbol->got_offset / 8; // The index in the .got table is the same as the index in the .rela.dyn table.
            if (i >= output_elf_file->rela_dyn_entry_count)
                panic("Symbol %s has a GOT offset that exceeds the size of .rela.dyn table: %d > %d",
                    symbol->name, i, output_elf_file->rela_dyn_entry_count);

            if (i > highest_got_entry) highest_got_entry = i;

            // Populate the relocation entry in .rela.dyn
            if (i >= output_elf_file->rela_dyn_entry_count) panic("Attempt to write beyond .rela.dyn at %d, size=%d", i, output_elf_file->rela_dyn_entry_count);
            ElfRelocation *r = &((ElfRelocation *) section_rela_dyn->data)[i];
            r->r_info = R_X86_64_RELATIVE;
            r->r_offset = got_value + symbol->got_offset;
            r->r_addend = symbol->dst_value;

            i++;
        }
    }

    // Add entries for symbols that need copying at runtime with a R_X86_64_COPY relocation
    map_ordered_foreach(global_symbol_table->defined_symbols, it) {
        const SymbolNV *snv = map_ordered_iterator_key(&it);
        Symbol *symbol = map_ordered_get(global_symbol_table->defined_symbols, snv);
        if (!symbol_is_in_dynsym(output_elf_file, symbol, snv)) continue;
        if (!(symbol->extra & SE_COPY_RELOCATION)) continue;

        if (i >= output_elf_file->rela_dyn_entry_count)
            panic("Symbol %s has a .rela.dyn offset that exceeds the size of .rela.dyn table: %d >= %d",
                symbol->name, i, output_elf_file->rela_dyn_entry_count);

        if (i >= output_elf_file->rela_dyn_entry_count) panic("Attempt to write beyond .rela.dyn at %d, size=%d", i, output_elf_file->rela_dyn_entry_count);
        ElfRelocation *r = &((ElfRelocation *) section_rela_dyn->data)[i];
        r->r_info = R_X86_64_COPY + ((uint64_t) symbol->dst_dynsym_index << 32);
        r->r_offset = symbol->dst_value;
        r->r_addend = 0;

        i++;
    }

    // Add extra dyn entries for R_X86_64_RELATIVE relocations
    for (int j = 0; j < output_elf_file->rela_dyn_R_X86_64_RELATIVE_relocations->length; j++) {
        RelativeRelaDynRelocation *rrdr = output_elf_file->rela_dyn_R_X86_64_RELATIVE_relocations->elements[j];

        uint64_t addend;

        if (rrdr->symbol) {
            addend = rrdr->symbol->dst_value + rrdr->addend;
        }
        else {
            // The symbol is an offset into the relocation input section
            if (!rrdr->relocation_input_section) panic("Unexpected undefined e->relocation_input_section");
            addend = rrdr->relocation_input_section->output_section->offset + rrdr->relocation_input_section->dst_offset + rrdr->addend;
        }

        if (i >= output_elf_file->rela_dyn_entry_count) panic("Attempt to write beyond .rela.dyn at %d, size=%d", i, output_elf_file->rela_dyn_entry_count);
        ElfRelocation *r = &((ElfRelocation *) section_rela_dyn->data)[i];
        r->r_info = R_X86_64_RELATIVE;
        r->r_offset = rrdr->target_section->output_section->offset + rrdr->target_section->dst_offset + rrdr->offset;
        r->r_addend = addend;

        i++;
    }

    // Add extra dyn entries for R_X86_64_64 relocations
    for (int j = 0; j < output_elf_file->rela_dyn_R_X86_64_64_relocations->length; j++) {
        RelativeRelaDynRelocation *rrdr = output_elf_file->rela_dyn_R_X86_64_64_relocations->elements[j];
        if (!rrdr->symbol) panic("Did not get a symbol for a R_X86_64_64 relocation\n");
        uint64_t addend = rrdr->addend;

        if (i >= output_elf_file->rela_dyn_entry_count) panic("Attempt to write beyond .rela.dyn at %d, size=%d", i, output_elf_file->rela_dyn_entry_count);
        ElfRelocation *r = &((ElfRelocation *) section_rela_dyn->data)[i];
        r->r_info = R_X86_64_64 + ((uint64_t) rrdr->symbol->dst_dynsym_index << 32);
        r->r_offset = rrdr->target_section->output_section->offset + rrdr->target_section->dst_offset + rrdr->offset;
        r->r_addend = addend;

        i++;
    }

    ElfSectionHeader *h = &output_elf_file->elf_section_headers[section_rela_dyn->output_section->index];
    h->sh_link = output_elf_file->section_dynsym->output_section->index;
    h->sh_entsize = sizeof(ElfRelocation);
    h->sh_flags = SHF_ALLOC;
}

// Allocate space for symbols used in a dynamic executable that are defined in a shared library.
void layout_data_copy_section(OutputElfFile *output_elf_file) {
    if (output_elf_file->type != ET_DYN || !output_elf_file->is_executable) return;

    InputSection *data_copy_section = NULL;
    map_ordered_foreach(global_symbol_table->defined_symbols, it) {
        const SymbolNV *snv = map_ordered_iterator_key(&it);
        Symbol *symbol = map_ordered_get(global_symbol_table->defined_symbols, snv);
        if (!symbol->resolves_undefined_symbol) continue;

        // The alignment of the symbol is the alignment of the section it's in.
        int align = symbol->input_section->align;

        // Create a new .data.copy section if not already existent.
        if (!data_copy_section)
            data_copy_section = get_or_create_extra_section(output_elf_file, DATA_COPY_SECTION_NAME, SHT_PROGBITS, SHF_ALLOC | SHF_WRITE, align);

        // Increase the alignment of the section if necessary
        if (align > data_copy_section->align) data_copy_section->align = align;

        // Align the section
        data_copy_section->size = ALIGN_UP(data_copy_section->size, align);

        // Disassociate the symbol with the shared library
        // This is a bit hacky, since this pretends the symbol originated in the executable in the first place
        // and completely removes any reference to the shared library.
        symbol->input_section = data_copy_section;
        symbol->src_value = data_copy_section->size;
        symbol->extra |= SE_COPY_RELOCATION;
        symbol->needs_dynsym_entry = 1;
        symbol->sources = SRC_OBJECT;

        data_copy_section->size += symbol->size;
    }

    if (data_copy_section)
        data_copy_section->data = calloc(1, data_copy_section->size);
}

// Any ELF file that has a PT_DYNAMIC section must add a local _DYNAMIC symbol.
// Add a _DYNAMIC symbol for ET_DYN outputs
void add_dynamic_symbol(OutputElfFile *output_elf_file) {
    if (output_elf_file->type == ET_DYN) {
        add_defined_symbol(global_symbol_table, DYNAMIC_SYMBOL_NAME, 0, STT_OBJECT, STB_LOCAL, 0, 0, SRC_INTERNAL);
        resolve_undefined_symbol(DYNAMIC_SYMBOL_NAME, 0, 0, 0, 0);
    }
}

// Create the .gnu.version_r section with filenames and names of all symbol versions
void make_verneed_section(OutputElfFile *output_elf_file) {
    if (output_elf_file->type != ET_DYN) return;

    int verneed_names_count = output_elf_file->verneed_names->length;
    if (verneed_names_count == 0) return;

    InputSection *extra_section = create_extra_section(output_elf_file, VERNEED_SECTION_NAME, SHT_GNU_VERNEED, SHF_ALLOC, 8);
    // The section header link will be set after output sections are created.

    if (DEBUG_SYMBOL_VERSIONS) printf("\nCreating .gnu.version_r entries:\n");

    // Make a map of version index to filename
    char **version_index_filenames = calloc(1, global_symbol_version_indexes_list->length * sizeof(char *));
    map_ordered_foreach(global_symbol_table->defined_symbols, it) {
        const SymbolNV *snv = map_ordered_iterator_key(&it);
        if (snv->version_index <= GLOBAL_SYMBOL_INDEX_DEFAULT) continue;
        if (version_index_filenames[snv->version_index]) continue;

        Symbol *symbol = map_ordered_get(global_symbol_table->defined_symbols, snv);
        if (!(symbol->sources & SRC_SHARED_LIBRARY)) continue;
        if (!symbol->src_elf_file || symbol->src_elf_file->type != ET_DYN) continue;

        // Get the basename of the lib
        char *filename = strdup(symbol->src_elf_file->filename);
        char *basename = strrchr(filename, '/');
        version_index_filenames[snv->version_index] = basename ? basename + 1 : filename;
    }

    // Make a map of library version names to a list of versions in that library
    StrMap *library_versions = new_strmap();
    List *libraries = new_list(8);

    for (int i = 0; i < verneed_names_count; i++) {
        char *version_name = output_elf_file->verneed_names->elements[i];
        int global_version_index = (int) (uint64_t) strmap_get(global_symbol_version_indexes_map, version_name);
        char *filename = version_index_filenames[global_version_index];

        List *versions = strmap_get(library_versions, filename);
        if (!versions) {
            versions = new_list(8);
            strmap_put(library_versions, filename, versions);
            append_to_list(libraries, filename);
        }
        append_to_list(versions, version_name);
    }

    for (int i = 0; i < libraries->length; i++) {
        char *filename = libraries->elements[i];
        List *versions = strmap_get(library_versions, filename);
        int versions_count = versions->length;

        if (DEBUG_SYMBOL_VERSIONS) printf("%s\n", filename);

        uint32_t entry_size = sizeof(ElfVerneed) + versions_count * sizeof(ElfVernaux);
        char *entry = allocate_in_section(extra_section, entry_size);
        memset(entry, 0, entry_size);

        ElfVerneed *vn = (ElfVerneed *) entry;
        ElfVernaux *vna = (ElfVernaux *) (entry + sizeof(ElfVerneed));

        vn->vn_version = 1;
        vn->vn_cnt = versions_count;
        vn->vn_file = add_dynstr_string(output_elf_file, filename);
        vn->vn_aux = sizeof(ElfVerneed);
        vn->vn_next = (i < libraries->length - 1) ? entry_size : 0;

        for (int j = 0; j < versions_count; j++) {
            char *version_name = versions->elements[j];
            int dynstr_index = add_dynstr_string(output_elf_file, version_name);

            int global_version_index = (int) (uint64_t) strmap_get(global_symbol_version_indexes_map, version_name);
            int output_version_index = output_elf_file->verneed_indexes[global_version_index];
            uint16_t vna_other = output_version_index;

            vna[j].vna_hash = elf_hash(version_name);
            vna[j].vna_flags = 0;
            vna[j].vna_other = vna_other;
            vna[j].vna_name = dynstr_index;
            vna[j].vna_next = (j + 1 < versions_count) ? sizeof(ElfVernaux) : 0;

            if (DEBUG_SYMBOL_VERSIONS) printf("   %3d %s\n", output_version_index, version_name);
        }
    }

    extra_section->info = libraries->length;

    for (int i = 0; i < libraries->length; i++) {
        char *needed_library = libraries->elements[i];
        List *versions = strmap_get(library_versions, needed_library);
        if (versions) free_list(versions);
    }

    free_list(libraries);
    free_strmap(library_versions);
    free(version_index_filenames);
}

// Create the .gnu.version section
void make_versym_section(OutputElfFile *output_elf_file) {
    if (output_elf_file->type != ET_DYN) return;
    if (!output_elf_file->section_dynsym) return;
    if (!output_elf_file->verneed_names || output_elf_file->verneed_names->length == 0) return;

    InputSection *section_versym = create_extra_section(output_elf_file, VERSYM_SECTION_NAME, SHT_GNU_VERSYM, SHF_ALLOC, 2);

    int entry_count = output_elf_file->dynsym_symbol_count + 1; // Include first null entry
    section_versym->size = entry_count * sizeof(uint16_t);
    section_versym->data = calloc(1, section_versym->size);

    if (DEBUG_SYMBOL_VERSIONS) printf("\nWriting %d .gnu.version entries\n", entry_count);

    uint16_t *entries = (uint16_t *) section_versym->data;

    map_ordered_foreach(global_symbol_table->defined_symbols, it) {
        const SymbolNV *snv = map_ordered_iterator_key(&it);
        Symbol *symbol = map_ordered_get(global_symbol_table->defined_symbols, snv);
        if (!symbol_is_in_dynsym(output_elf_file, symbol, snv)) continue;

        uint16_t version_index = VER_NDX_GLOBAL;

        if (snv->version_index > GLOBAL_SYMBOL_INDEX_DEFAULT) {
            int output_version_index = output_elf_file->verneed_indexes[snv->version_index];
            if (!output_version_index)
                panic("Missing verneed index for symbol version %d (%s)", snv->version_index, snv->full_name);

            version_index = (uint16_t) output_version_index;
        }

        if (DEBUG_SYMBOL_VERSIONS) {
            char *version_name = global_symbol_version_indexes_list->elements[version_index];
            printf("%3d: %-16s %3d  %s\n", symbol->dst_dynsym_index, version_name, version_index, snv->name);
        }
        entries[symbol->dst_dynsym_index] = version_index;
    }
}
