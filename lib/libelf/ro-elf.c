#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "list.h"
#include "ro-elf.h"
#include "strmap.h"
#include "error.h"

// Open an ELF file and read the ELF header
static ElfFile *open_file(const char *filename) {
    ElfFile *elf_file = malloc(sizeof(ElfFile));

    elf_file->filename = strdup(filename);

    elf_file->file = fopen(filename, "r");

    if (elf_file->file == 0) {
        perror(filename);
        exit(1);
    }

    elf_file->elf_header = malloc(sizeof(ElfHeader));
    int read = fread(elf_file->elf_header, 1, sizeof(ElfHeader), elf_file->file);
    if (read != sizeof(ElfHeader)) error("Unable to read input file: %s", filename);

    return elf_file;
}

// Read from the file into a buffer
static char *read_from_file(ElfFile *elf_file, void *dst, uint64_t offset, uint64_t size) {
    fseek(elf_file->file, offset, SEEK_SET);
    int read = fread(dst, 1, size, elf_file->file);
    if (read != size) error("Unable to read input file: %s", elf_file->filename);
}

// Read a section into dst
void load_section_into_buffer(ElfFile *elf_file, int section_index, void *dst) {
    ElfSectionHeader *section_header = &elf_file->section_headers[section_index];
    read_from_file(elf_file, dst, section_header->sh_offset, section_header->sh_size);
}

// Allocate memory for a section, read it, and return it
static void *load_section(ElfFile *elf_file, int section_index) {
    ElfSectionHeader *section_header = &elf_file->section_headers[section_index];
    void *result = malloc(section_header->sh_size);
    load_section_into_buffer(elf_file, section_index, result);

    return result;
}

// Ensure the file is an x86_64 ELF object file
static void check_file(ElfFile *elf_file) {
    ElfHeader *elf_header = elf_file->elf_header;

    // Ensure it's an elf file
        if (elf_header->ei_magic0 != 0x7f ||
            elf_header->ei_magic1 != 'E'  ||
            elf_header->ei_magic2 != 'L'  ||
            elf_header->ei_magic3 != 'F')
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
    if (elf_header->ei_osabi != ELF_OSABI_NONE)
        error("Invalid ABI version %d for file: %s", elf_header->ei_osabi, elf_file->filename);

    if (elf_header->e_type != ET_REL)
        error("ELF file isn't relocatable: %s", elf_file->filename);

    if (elf_header->e_machine != E_MACHINE_TYPE_X86_64)
        error("ELF file isn't for x86_64: %s", elf_file->filename);
}

// May return NULL if not existent
static Section *get_input_section(ElfFile *elf_file, char *name) {
    return strmap_get(elf_file->section_map, name);
}

static void load_sections(ElfFile *elf_file) {
    // Init section caches
    elf_file->section_list = new_list(32);
    elf_file->section_map = new_strmap();

    // Check the header size is within what we expect
    if (elf_file->elf_header->e_shentsize > sizeof(ElfSectionHeader))
        error("ELF section header size is too large for: %s", elf_file->filename);

    // Load all section headeres
    int section_headers_size = sizeof(ElfSectionHeader) * elf_file->elf_header->e_shnum;
    elf_file->section_headers = malloc(section_headers_size);
    read_from_file(elf_file, elf_file->section_headers, elf_file->elf_header->e_shoff, section_headers_size);

    // Load the section header strings
    elf_file->section_header_strings = load_section(elf_file, elf_file->elf_header->e_shstrndx);

    // Loop over all sections and populate the section headers list and map
    for (int i = 0; i < elf_file->elf_header->e_shnum; i++) {
        ElfSectionHeader *elf_section_header = &elf_file->section_headers[i];
        const char *name = &elf_file->section_header_strings[elf_section_header->sh_name];

        Section *section = calloc(1, sizeof(Section));
        section->index = i;
        section->elf_section_header = elf_section_header;
        if (elf_section_header->sh_type != SHT_NULL) {
            append_to_list(elf_file->section_list, section);
            strmap_put(elf_file->section_map, strdup(name), section);
        }
    }

    // Look up string table
    Section *strtab_section = get_input_section(elf_file, ".strtab");
    if (!strtab_section)
        error("No .strtab section in: %s", elf_file->filename);
    elf_file->strtab_strings = load_section(elf_file, strtab_section->index);

    // Look up symbol table
    Section *symtab_section = get_input_section(elf_file, ".symtab");
    if (!symtab_section)
        error("No .symtab section in: %s", elf_file->filename);

    elf_file->symbol_table = load_section(elf_file, symtab_section->index);
    elf_file->symbol_count = symtab_section->elf_section_header->sh_size / sizeof(ElfSymbol);
}

ElfFile *open_elf_file(const char *filename) {
    ElfFile *elf_file = open_file(filename);
    check_file(elf_file);
    load_sections(elf_file);

    return elf_file;
}
