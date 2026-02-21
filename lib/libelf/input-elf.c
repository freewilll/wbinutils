#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "list.h"
#include "input-elf.h"
#include "strmap.h"
#include "error.h"

#define DEBUG_SYMBOL_VERSIONS 0

static int read_header(InputElfFile *elf_file, int fail_on_read_error) {
    fseek(elf_file->file, elf_file->file_offset, SEEK_SET);
    elf_file->elf_header = malloc(sizeof(ElfHeader));

    int read = fread(elf_file->elf_header, 1, sizeof(ElfHeader), elf_file->file);
    if (read != sizeof(ElfHeader)) {
        if (fail_on_read_error) error("Unable to read ELF header from input file: %s", elf_file->filename);
        return 0;
    }

    elf_file->type = elf_file->elf_header->e_type;

    return 1;
}

// Read from the file into a buffer
static void *read_from_file(InputElfFile *elf_file, void *dst, uint64_t offset, uint64_t size) {
    fseek(elf_file->file, elf_file->file_offset + offset, SEEK_SET);
    int read = fread(dst, 1, size, elf_file->file);
    if (read != size) error("Unable to read from input ELF file: %s", elf_file->filename);
}

// Allocate memory for a section, read it, and return it
void *load_section_uncached(InputElfFile *elf_file, int section_index) {
    ElfSectionHeader *section_header = &elf_file->section_headers[section_index];
    void *result = malloc(section_header->sh_size);
    read_from_file(elf_file, result, section_header->sh_offset, section_header->sh_size);

    return result;
}

// Unless already done, allocate memory for a section, read it, and return it
void *load_section(InputElfFile *elf_file, InputSection *section) {
    if (section->data) return section->data;
    section->data = malloc(section->size);
    read_from_file(elf_file, section->data, section->src_offset, section->size);

    return section->data;
}

// Allocate space at the end of a section and return a pointer to it.
// Dynamically allocate space size as needed.
void *allocate_in_section(InputSection *section, int size) {
    int new_section_size = section->size + size;
    if (new_section_size > section->allocated) {
        if (!section->allocated) section->allocated = 1;
        while (new_section_size > section->allocated) section->allocated *= 2;
        section->data = realloc(section->data, section->allocated);
    }

    void *result = section->data + section->size;
    section->size = new_section_size;

    return result;
}

// Copy src to the end of a section and return the offset
int add_to_input_section(InputSection *section, const void *src, int size) {
    void *data = allocate_in_section(section, size);
    memcpy(data, src, size);
    return data - section->data;
}

static int has_elf_magic(InputElfFile *elf_file) {
    ElfHeader *elf_header = elf_file->elf_header;

    return (elf_header->ei_magic0 == 0x7f &&
        elf_header->ei_magic1 == 'E'  &&
        elf_header->ei_magic2 == 'L'  &&
        elf_header->ei_magic3 == 'F');
}

// Ensure the file is an x86_64 ELF object file
static void check_file(InputElfFile *elf_file) {
    ElfHeader *elf_header = elf_file->elf_header;

    if (!has_elf_magic(elf_file))
        error("Not an elf file: %s", elf_file->filename);

    // 32 vs bit
    if (elf_header->ei_class != ELF_CLASS_64)
        error("Not a 64-bit ELF file: %s", elf_file->filename);

    // Endianness
    if (elf_header->ei_data != ELF_DATA_2_LSB)
        error("Not a little-endian ELF file: %s", elf_file->filename);

    // ELF versions
    if (elf_header->ei_version != 1)
        error("Invalid ELF version %d for file: %s", elf_header->ei_version, elf_file->filename);
    if (elf_header->e_version != EV_CURRENT)
        error("Invalid ELF version %d for file: %s", elf_header->ei_version, elf_file->filename);

    // ABI version
    if (elf_header->ei_osabi != ELF_OSABI_NONE && elf_header->ei_osabi != ELF_OSABI_GNU)
        error("Invalid ABI version %d for file: %s", elf_header->ei_osabi, elf_file->filename);

    if (elf_header->e_type != ET_REL && elf_header->e_type != ET_DYN)
        error("ELF file isn't an object or shared-object file: %s", elf_file->filename);

    if (elf_header->e_machine != E_MACHINE_TYPE_X86_64)
        error("ELF file isn't for x86_64: %s", elf_file->filename);
}

// May return NULL if not existent
InputSection *get_input_section(InputElfFile *elf_file, char *name) {
    return strmap_get(elf_file->section_map, name);
}

static void load_section_headers(InputElfFile *elf_file) {
    // Load all section headeres
    int section_headers_size = sizeof(ElfSectionHeader) * elf_file->elf_header->e_shnum;
    elf_file->section_headers = malloc(section_headers_size);
    read_from_file(elf_file, elf_file->section_headers, elf_file->elf_header->e_shoff, section_headers_size);

    // Load the section header strings
    elf_file->section_header_strings = load_section_uncached(elf_file, elf_file->elf_header->e_shstrndx);

    // Loop over all sections and populate the section headers list and map
    for (int i = 0; i < elf_file->elf_header->e_shnum; i++) {
        ElfSectionHeader *elf_section_header = &elf_file->section_headers[i];
        char *name = &elf_file->section_header_strings[elf_section_header->sh_name];

        InputSection *section = calloc(1, sizeof(InputSection));
        section->name = name;
        section->index = i;
        section->size = elf_section_header->sh_size;
        section->type = elf_section_header->sh_type;
        section->flags = elf_section_header->sh_flags;
        section->info = elf_section_header->sh_info;
        section->link = elf_section_header->sh_link;
        section->align = elf_section_header->sh_addralign;
        section->src_offset = elf_section_header->sh_offset;

        append_to_list(elf_file->section_list, section);
        strmap_put(elf_file->section_map, strdup(name), section);
    }
}

static void load_symbol_table(InputElfFile *elf_file) {
    // Look up symbol table.
    // If a .dynsym exists, which is the case for .so files, use that. Otherwise,
    // fall back to .symtab, if present.
    InputSection *dynsym_section = get_input_section(elf_file, ".dynsym");
    InputSection *symtab_section = get_input_section(elf_file, ".symtab");
    InputSection *symbols_section = dynsym_section ? dynsym_section : (symtab_section ? symtab_section : NULL);

    if (symbols_section) {
        elf_file->symbol_table = load_section_uncached(elf_file, symbols_section->index);
        elf_file->symbol_count = symbols_section->size / sizeof(ElfSymbol);

        InputSection *strings_section = elf_file->section_list->elements[symbols_section->link];
        char *strings = load_section(elf_file, strings_section);
        elf_file->symbol_table_strings = strings;
    }
    else {
        elf_file->symbol_table = NULL;
        elf_file->symbol_count = 0;
        elf_file->symbol_table_strings = NULL;
    }
}

static void load_symbol_versions(InputElfFile *elf_file) {
    elf_file->symbol_table_version_indexes = NULL;
    elf_file->symbol_version_names = new_list(32);

    // Look up symbol version indexes
    InputSection *gnu_version = get_input_section(elf_file, ".gnu.version");
    if (!gnu_version) return;
    load_section(elf_file, gnu_version);

    uint16_t *gnu_version_data = gnu_version->data;

    elf_file->symbol_table_version_indexes = calloc(elf_file->symbol_count, sizeof(uint16_t));
    elf_file->non_default_versioned_symbols = calloc(elf_file->symbol_count, sizeof(int));
    elf_file->global_version_indexes = calloc(elf_file->symbol_count, sizeof(int));

    for (int i = 0; i < elf_file->symbol_count; i++) {
        uint16_t version_index = gnu_version_data[i];
        int is_hidden = version_index & VERSYM_HIDDEN;
        if (is_hidden) elf_file->non_default_versioned_symbols[i] = 1;
        version_index &= ~VERSYM_HIDDEN; // Mask off hidden bit
        elf_file->symbol_table_version_indexes[i] = version_index;
    }

    // Parse VERDEF section
    InputSection *gnu_version_d = get_input_section(elf_file, ".gnu.version_d");
    if (gnu_version_d) {
        if (DEBUG_SYMBOL_VERSIONS) printf("Verdef for %s:\n", elf_file->filename);

        load_section(elf_file, gnu_version_d);
        void *gnu_version_d_data = gnu_version_d->data;

        ElfVerdef *vd = gnu_version_d_data;
        ElfVerdef *vd_end = gnu_version_d_data + gnu_version_d->size;

        while (vd < vd_end) {
            if (vd->vd_version != 1)
                panic("Unsupported GNU verdef version %d", vd->vd_version);
            if (vd->vd_cnt == 0)
                panic("Invalid verdef with zero vd_cnt");

            int is_base = vd->vd_flags & VER_FLG_BASE; // Filename

            if (!is_base) {
                // The first entry of vd_aux is the version name.
                // The rest are parents, which aren't dealt with here.
                int vda_offset = vd->vd_aux;

                ElfVerdaux *vda = (void *) vd + vda_offset;
                const char *version_name = &((char *) elf_file->symbol_table_strings)[vda->vda_name];

                // Ensure the symbol_version_names is large enough to hold the index/string
                if (elf_file->symbol_version_names->length < vd->vd_ndx + 1)
                    resize_list(elf_file->symbol_version_names, vd->vd_ndx + 1);

                elf_file->symbol_version_names->elements[vd->vd_ndx] = strdup(version_name);

                if (DEBUG_SYMBOL_VERSIONS)
                    printf("idx %5d: flags=%10x name=%s\n",
                        vd->vd_ndx,      // Version Index
                        vd->vd_flags,    // Version information
                        version_name
                    );
            }

            if (!vd->vd_next) break;
            vd =(void *) vd + vd->vd_next;
        }
    }

    // Parse VERNEED section
    InputSection *gnu_version_r = get_input_section(elf_file, ".gnu.version_r");
    if (gnu_version_r) {
        load_section(elf_file, gnu_version_r);
        void *gnu_version_r_data = gnu_version_r->data;

       if (DEBUG_SYMBOL_VERSIONS)  printf("Verneed for %s:\n", elf_file->filename);

        ElfVerneed *vn = gnu_version_r_data;
        ElfVerneed *vn_end = gnu_version_r_data + gnu_version_r->size;

        while (vn < vn_end) {
            if (vn->vn_version != 1)
                panic("Unsupported GNU verneed version %d", vn->vn_version);
            if (vn->vn_cnt == 0)
                panic("Invalid verneed with zero vn_cnt");

            if (DEBUG_SYMBOL_VERSIONS)
                printf("Addr %010lx  ver=%10x file=%10x cnt=%10x aux=%10x next=%10x\n",
                    (void *) vn - gnu_version_r_data,
                    vn->vn_version,  // Version revision
                    vn->vn_file,     // Offset of filename for this dependency
                    vn->vn_cnt,      // Number of associated aux entries
                    vn->vn_aux,      // Offset in bytes to verdaux array
                    vn->vn_next      // Offset in bytes to next verneed entry
                );

            // Loop over all aux entries.
            int vna_offset = vn->vn_aux;
            for (int i = 0; i < vn->vn_cnt; i++) {
                if (vna_offset >= gnu_version_r->size) panic("VNA offset %d exceeds size %x", vna_offset, gnu_version_r->size);
                ElfVernaux *vna = (void *) vn + vna_offset;
                const char *version_name = &((char *) elf_file->symbol_table_strings)[vna->vna_name];
                uint16_t version_index = vna->vna_other;
                if (DEBUG_SYMBOL_VERSIONS) printf("  %s version= %d\n", version_name, version_index);

                // Ensure the symbol_version_names is large enough to hold the index/string
                if (elf_file->symbol_version_names->length < version_index + 1)
                    resize_list(elf_file->symbol_version_names, version_index + 1);

                elf_file->symbol_version_names->elements[version_index] = strdup(version_name);

                if (!vna->vna_next) break;
                vna_offset += vna->vna_next;
            }

            if (!vn->vn_next) break;
            vn =(void *) vn + vn->vn_next;
        }
        if (DEBUG_SYMBOL_VERSIONS) printf("\n");
    }
}

static void load_sections(InputElfFile *elf_file) {
    // Init section caches
    elf_file->section_list = new_list(32);
    elf_file->section_map = new_strmap();

    // Check the header size is within what we expect
    if (elf_file->elf_header->e_shentsize > sizeof(ElfSectionHeader))
        error("ELF section header size is too large for: %s", elf_file->filename);

    load_section_headers(elf_file);
    load_symbol_table(elf_file);
    load_symbol_versions(elf_file);
}

static void read_common_file_data(InputElfFile *elf_file) {
    read_header(elf_file, 1);
    check_file(elf_file);
    load_sections(elf_file);
}

// Open an object file but don't do anything with it
static InputElfFile *open_elf_file_internal(const char *filename) {
    InputElfFile *elf_file = calloc(1, sizeof(InputElfFile));
    elf_file->filename = strdup(filename);
    elf_file->file = fopen(filename, "r");

    if (elf_file->file == 0) {
        perror(filename);
        exit(1);
    }

    return elf_file;
}

// Open an object file
InputElfFile *open_elf_file(const char *filename) {
    InputElfFile *elf_file = open_elf_file_internal(filename);
    read_common_file_data(elf_file);
    return elf_file;
}

// Open an object file in an archive
InputElfFile *open_elf_file_in_archive(FILE *file, const char *filename, int offset) {
    InputElfFile *elf_file = calloc(1, sizeof(InputElfFile));
    elf_file->filename = filename ? strdup(filename) : NULL;
    elf_file->file = file;
    elf_file->file_offset = offset;

    read_common_file_data(elf_file);

    return elf_file;
}

int file_is_shared_library_file(const char *filename) {
    InputElfFile *elf_file = open_elf_file_internal(filename);
    if (!read_header(elf_file, 0)) return 0;
    if (!has_elf_magic(elf_file)) return 0;
    return elf_file->elf_header->e_type == ET_DYN;
}

// Print readelf compatible symbol table output
void dump_symbols(InputElfFile *elf_file) {
    if (elf_file->symbol_count && !elf_file->symbol_table)
        panic("There are symbols, yet no symbol table");

    printf("Symbol Table:\n");
    printf("   Num:    Value          Size Type    Bind   Vis        Ndx Name\n");

    for (int i = 0; i < elf_file->symbol_count; i++) {
        ElfSymbol *symbol = &elf_file->symbol_table[i];

        char binding = (symbol->st_info >> 4) & 0xf;
        char type = symbol->st_info & 0xf;
        char visibility = symbol->st_other & 3;
        const char *type_name = SYMBOL_TYPE_NAMES[type];
        const char *binding_name = SYMBOL_BINDING_NAMES[binding];
        const char *visibility_name = SYMBOL_VISIBILITY_NAMES[visibility];

        printf("%6d: %016ld  %4ld %-8s%-7s%-9s  ", i, symbol->st_value, symbol->st_size, type_name, binding_name, visibility_name);
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
            printf(" %s\n", &elf_file->symbol_table_strings[strtab_offset]);
        else
            printf("\n");
    }
}
