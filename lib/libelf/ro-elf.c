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

static InputElfFile *read_file(const char *filename) {
    InputElfFile *elf_file = malloc(sizeof(InputElfFile));

    elf_file->filename = strdup(filename);

    FILE *f  = fopen(filename, "r");

    if (f == 0) {
        perror(filename);
        exit(1);
    }

    fseek(f, 0, SEEK_END);
    elf_file->size = ftell(f);
    fseek(f, 0, SEEK_SET);

    elf_file->data = malloc(elf_file->size);
    int read = fread(elf_file->data, 1, elf_file->size, f);
    if (read != elf_file->size) {
        error("Unable to read input file: %s\n", filename);
        exit(1);
    }

    fclose(f);

    elf_file->elf_header = (ElfHeader *) elf_file->data;

    return elf_file;
}

// Ensure the file is an x86_64 ELF object file
static void check_file(InputElfFile *elf_file) {
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
static InputSection *get_input_section(InputElfFile *elf_file, char *name) {
    return strmap_get(elf_file->section_map, name);
}

static void load_sections(InputElfFile *elf_file) {
    elf_file->section_list = new_list(32);
    elf_file->section_map = new_strmap();

    // Check the header size is within what we expect
    if (elf_file->elf_header->e_shentsize > sizeof(ElfSectionHeader))
        error("ELF section header size is too large for: %s", elf_file->filename);

    uintptr_t section_headers_offset = elf_file->elf_header->e_shoff;

    // Get pointer to the section header strings
    uintptr_t section_header_strings_offset = section_headers_offset + elf_file->elf_header->e_shstrndx * elf_file->elf_header->e_shentsize;
    ElfSectionHeader *section_header  = (ElfSectionHeader*) (elf_file->data + section_header_strings_offset);
    elf_file->section_header_strings = elf_file->data + section_header->sh_offset;

    // Loop over all sections and populate the section headers list and map
    for (int i = 0; i < elf_file->elf_header->e_shnum; i++) {
        uintptr_t offset = section_headers_offset + i * elf_file->elf_header->e_shentsize;

        ElfSectionHeader *elf_section_header  = (ElfSectionHeader*) (elf_file->data + offset);
        const char *name = &elf_file->section_header_strings[elf_section_header->sh_name];

        InputSection *section = calloc(1, sizeof(InputSection));
        section->elf_section_header = elf_section_header;
        if (elf_section_header->sh_type != SHT_NULL) {
            append_to_list(elf_file->section_list, section);
            strmap_put(elf_file->section_map, strdup(name), section);
        }
    }

    // Look up string table
    InputSection *strtab_section = get_input_section(elf_file, ".strtab");
    if (!strtab_section)
        error("No .strtab section in: %s", elf_file->filename);
    elf_file->strtab_strings = elf_file->data + strtab_section->elf_section_header->sh_offset;

    // Look up symbol table
    InputSection *symtab_section = get_input_section(elf_file, ".symtab");
    if (!symtab_section)
        error("No .symtab section in: %s", elf_file->filename);
    elf_file->symbol_table = (ElfSymbol *) elf_file->data + symtab_section->elf_section_header->sh_offset;
    elf_file->symbol_count = symtab_section->elf_section_header->sh_size / sizeof(ElfSymbol);
}

InputElfFile *read_elf_file(const char *filename) {
    InputElfFile *elf_file = read_file(filename);
    check_file(elf_file);
    load_sections(elf_file);

    return elf_file;
}
