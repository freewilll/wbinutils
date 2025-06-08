#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "elf.h"
#include "error.h"
#include "list.h"
#include "strmap.h"
#include "output-elf.h"

// Get a RW section. May return NULL if not existent
OutputSection *get_output_section(OutputElfFile *elf_file, const char *name) {
    return strmap_get(elf_file->sections_map, name);
}

// Add a read/write section to an elf file
OutputSection *add_output_section(OutputElfFile *output_elf_file, const char *name, int type, int flags, int align) {
    OutputSection *section = calloc(1, sizeof(OutputSection));
    section->name = strdup(name);
    section->type = type;
    section->flags = flags;
    section->align = align;

    append_to_list(output_elf_file->sections_list, section);
    strmap_put(output_elf_file->sections_map, name, section);

    return section;
}

// Move output_section, which must be the last section in the list, to position
void move_output_section(OutputElfFile *output_elf_file, OutputSection *output_section, int position) {
    List *old_sections_list = output_elf_file->sections_list;

    // Check that the target position cannot be the initial NULL or first section
    if (position < 2)
        panic("Position %d too small %d", position, position);

    // Check that the target position is within the range
    if (position >= old_sections_list->length)
        panic("Position %d exceeds amount of sections %d", position, old_sections_list->length);

    // Check output_section is the last one
    OutputSection *last_output_section = old_sections_list->elements[old_sections_list->length - 1];
    if (last_output_section != output_section)
        panic("Section %s is not the last section", output_section->name);

    // Check we're not moving the section onto itself
    if (old_sections_list->elements[position] == output_section)
        panic("Section %s is already where it needs to be", output_section->name);

    // Recreate the sections list
    List *new_sections_list = new_list(old_sections_list->length);

    for (int i = 0; i < position; i++)
        append_to_list(new_sections_list, old_sections_list->elements[i]);

    append_to_list(new_sections_list, output_section);

    for (int i = position; i < old_sections_list->length - 1; i++)
        append_to_list(new_sections_list, old_sections_list->elements[i]);

    free_list(old_sections_list);

    output_elf_file->sections_list = new_sections_list;
}

// Allocate space at the end of a section and return a pointer to it.
// Dynamically allocate space size as needed.
static void *allocate_in_section(OutputSection *section, int size) {
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
int add_to_output_section(OutputSection *section, const void *src, int size) {
    char *data = allocate_in_section(section, size);
    memcpy(data, src, size);
    return data - section->data;
}

// Add size repeated characters to the section and return the offset
int add_repeated_value_to_output_section(OutputSection *section, char value, int size) {
    char *data = allocate_in_section(section, size);
    memset(data, value, size);
    return data - section->data;
}

// Add size zeros to the section and return the offset
int add_zeros_to_output_section(OutputSection *section, int size) {
    return add_repeated_value_to_output_section(section, 0, size);
}

// Add a symbol to the ELF symbol table symtab
// This function must be called with all local symbols first, then all global symbols
int add_elf_symbol(OutputElfFile *output_elf_file, const char *name, long value, long size, int binding, int type, int visibility, int section_index) {
    // Add a string to the strtab unless name is "".
    // Empty names are all mapped to the first entry in the string table.
    int strtab_offset = *name ? add_to_output_section(output_elf_file->section_strtab, name, strlen(name) + 1) : 0;

    ElfSymbol *symbol = allocate_in_section(output_elf_file->section_symtab, sizeof(ElfSymbol));
    memset(symbol, 0, sizeof(ElfSymbol));
    symbol->st_name = strtab_offset;
    symbol->st_value = value;
    symbol->st_size = size;
    symbol->st_info = (binding << 4) + type;
    symbol->st_other = visibility;
    symbol->st_shndx = section_index;

    int index = symbol - (ElfSymbol *) output_elf_file->section_symtab->data;

    if (binding == STB_LOCAL) output_elf_file->local_symbol_end = index;

    return index;
}

// Add a special symbol with the source filename
void add_file_symbol(OutputElfFile *output_elf_file, char *filename) {
    add_elf_symbol(output_elf_file, filename, 0, 0, STB_LOCAL, STT_FILE, STV_DEFAULT, SHN_ABS);
}

// Add a relocation to the ELF rela_text section.
void add_elf_relocation(OutputElfFile *output_elf_file, OutputSection *section, int type, int symbol_index, long offset, long addend) {
    ElfRelocation *r = allocate_in_section(section, sizeof(ElfRelocation));

    r->r_offset = offset;
    r->r_info = type + ((long) symbol_index << 32);
    r->r_addend = addend;
}

// Create a new OutputElfFile object and set some defaults
OutputElfFile *new_output_elf_file(const char *filename, int type) {
    OutputElfFile *result = calloc(1, sizeof(OutputElfFile));

    result->filename = strdup(filename);
    result->type = type;
    result->sections_list = new_list(32);
    result->sections_map = new_strmap();

    return result;
}

// Make an ELF header
void make_elf_headers(OutputElfFile *output) {
    // The layout the first page as follows:
    // - ELF header
    // - Program segment headers (if an executable)
    // - Section headers

    int e_phentsize = (output->type == ET_EXEC || output->type == ET_DYN) ? sizeof(ElfProgramSegmentHeader) : 0;

    // ELF header
    ElfHeader *elf_header = (ElfHeader *) output->data;

    elf_header->ei_magic0   = 0x7f;                               // Magic
    elf_header->ei_magic1   = 'E';
    elf_header->ei_magic2   = 'L';
    elf_header->ei_magic3   = 'F';
    elf_header->ei_class    = ELF_CLASS_64;                        // 64-bit
    elf_header->ei_data     = ELF_DATA_2_LSB;                      // LSB
    elf_header->ei_version  = 1;                                   // Original ELF version
    elf_header->ei_osabi    = ELF_OSABI_GNU;                       // GNU ELF extensions are supported, like IFUNCs.
    elf_header->e_type      = output->type;                        // Object type
    elf_header->e_machine   = E_MACHINE_TYPE_X86_64;               // x86-64
    elf_header->e_version   = EV_CURRENT;                          // EV_CURRENT Current version of ELF
    elf_header->e_entry     = output->entrypoint;                  // Executable prograam entrypoint
    elf_header->e_phoff     = output->elf_program_segments_offset; // Offset to program header table
    elf_header->e_shoff     = output->elf_section_headers_offset;  // Offset to section header table
    elf_header->e_ehsize    = sizeof(ElfHeader);                   // The size of this header, 0x40 for 64-bit
    elf_header->e_phentsize = e_phentsize;                         // The size of a program header
    elf_header->e_phnum     = output->elf_program_segments_count;  // Number of program header entries
    elf_header->e_shentsize = sizeof(ElfSectionHeader);            // The size of a section header
    elf_header->e_shnum     = output->sections_list->length;       // Number of section header entries
    elf_header->e_shstrndx  = output->section_shstrtab->index;     // The section header string table index

    // Copy section headers
    memcpy(output->data + output->elf_section_headers_offset, output->elf_section_headers, output->elf_section_headers_size);
}

// If both are present, ensure that the .tdata and .tbss sections are consecutive
static void place_tls_sections(List *sections_list) {
    int tdata_section_index = 0;
    int tbss_section_index = 0;

    for (int i = 0; i < sections_list->length; i++) {
        OutputSection *section = sections_list->elements[i];
        if (!strcmp(section->name, ".tdata")) tdata_section_index = i;
        if (!strcmp(section->name, ".tbss")) tbss_section_index = i;
    }

    if (!tdata_section_index || !tbss_section_index || tdata_section_index == tbss_section_index - 1) return;

    // Ensure the .tdata section comes first
    if (tdata_section_index > tbss_section_index) {
        OutputSection *tmp = sections_list->elements[tdata_section_index];
        sections_list->elements[tdata_section_index] = sections_list->elements[tbss_section_index];
        sections_list->elements[tbss_section_index] = tmp;

        int tmp2 = tdata_section_index;
        tdata_section_index = tbss_section_index;
        tbss_section_index = tmp2;

        // Update the section indexes to match their new positions
        ((OutputSection *) sections_list->elements[tdata_section_index])->index = tdata_section_index;
        ((OutputSection *) sections_list->elements[tbss_section_index])->index = tbss_section_index;
    }

    // The .tdata section is now not last. Swap the next section with the .tbss section
    OutputSection *tmp = sections_list->elements[tdata_section_index + 1];
    sections_list->elements[tdata_section_index + 1] = sections_list->elements[tbss_section_index];
    sections_list->elements[tbss_section_index] = tmp;

    // Update the section indexes to match their new positions
    ((OutputSection *) sections_list->elements[tdata_section_index + 1])->index = tdata_section_index + 1;
    ((OutputSection *) sections_list->elements[tbss_section_index])->index = tbss_section_index;
}

// Rearrange sections list so that .symtab, .strtab and .shstrtab are last.
// Then set the index.
void make_section_indexes(OutputElfFile *output_elf_file) {
    List *sections_list = output_elf_file->sections_list;

    List *new_sections_list = new_list(sections_list->length);
    List *selected_sections_list = new_list(3);

    for (int i = 0; i < sections_list->length; i++) {
        OutputSection *section = sections_list->elements[i];

        if (!strcmp(section->name, ".symtab") || !strcmp(section->name, ".strtab") || !strcmp(section->name, ".shstrtab"))
            append_to_list(selected_sections_list, section);
        else
            append_to_list(new_sections_list, section);
    }

    free_list(sections_list);

    int non_selected_sections_length = new_sections_list->length;

    // Set the index on the first sections
    for (int i = 0; i < non_selected_sections_length; i++) {
        OutputSection *section = new_sections_list->elements[i];
        section->index = i;
    }

    // Append the remaining sections
    for (int i = 0; i < selected_sections_list->length; i++) {
        OutputSection *section = selected_sections_list->elements[i];
        section->index = non_selected_sections_length + i;
        append_to_list(new_sections_list, section);
    }

    free_list(selected_sections_list);

    place_tls_sections(new_sections_list);

    output_elf_file->sections_list = new_sections_list;
}

uint64_t headers_size(OutputElfFile *elf_file) {
    return
        sizeof(ElfHeader) +                              // Elf header
        elf_file->elf_program_segments_header_size +     // Program segments headers
        elf_file->elf_section_headers_size;              // Section headers
}

// Make an ELF section header
void make_output_section_header(OutputElfFile *output_elf_file, ElfSectionHeader *sh, OutputSection *section) {
    sh->sh_name      = add_to_output_section(output_elf_file->section_shstrtab, (char *) section->name, strlen(section->name) + 1);
    sh->sh_type      = section->type;
    sh->sh_flags     = section->flags;
    sh->sh_offset    = section->offset;
    sh->sh_size      = section->size;
    sh->sh_link      = section->link;
    sh->sh_info      = section->info;
    sh->sh_addralign = section->align;
    sh->sh_entsize   = section->entsize;
}

// Make all output section headers
void make_output_section_headers(OutputElfFile *output_elf_file) {
    // Allocate ememory
    output_elf_file->elf_section_headers_size = sizeof(ElfSectionHeader) * output_elf_file->sections_list->length;
    output_elf_file->elf_section_headers = calloc(1, output_elf_file->elf_section_headers_size);

    // Loop over all headers
    for (int i = 0; i < output_elf_file->sections_list->length; i++) {
        OutputSection *section = output_elf_file->sections_list->elements[i];
        make_output_section_header(output_elf_file, &output_elf_file->elf_section_headers[i], section);
    }
}

// Given the sizes and alignments of all output sections, determine the offsets in the final ELF file,
// and allocate memory for the output
void layout_output_elf_sections(OutputElfFile *output_elf_file) {
    ElfSectionHeader *elf_section_headers = output_elf_file->elf_section_headers;

    // The layout the first page as follows:
    // - ELF header
    // - Program segment headers
    // - Section headers

    if (output_elf_file->type == ET_EXEC) {
        output_elf_file->elf_program_segments_offset  = sizeof(ElfSectionHeader);
        output_elf_file->elf_section_headers_offset  = output_elf_file->elf_program_segments_offset + output_elf_file->elf_program_segments_header_size;
    }
    else {
        output_elf_file->elf_section_headers_offset  = sizeof(ElfSectionHeader);
    }

    // Determine section offsets
    // Align start of the sections on a page boundary after the ELF, program segments and section headers
    int offset =
        sizeof(ElfHeader) +                                     // Elf header
        output_elf_file->elf_program_segments_header_size +     // Program segments headers
        output_elf_file->elf_section_headers_size;              // Section headers

    // Loop over all output sections
    for (int i = 1; i < output_elf_file->sections_list->length; i++) {
        OutputSection *section = output_elf_file->sections_list->elements[i];

        // Align program sections to page boundaries if it's an executable or library
        if ((output_elf_file->type == ET_EXEC || output_elf_file->type == ET_DYN) && section->type == SHT_PROGBITS)
            offset = ALIGN_UP(offset, 0x1000);

        section->offset = offset;
        elf_section_headers[i].sh_offset = offset;

        if (section->type != SHT_NOBITS)
            offset = ALIGN_UP(offset + section->size, 16);

        // Save the total size of the .tdata + .tbss sections
        if (section->type == SHT_NOBITS && (section->flags & SHF_TLS)) {
            output_elf_file->tls_template_size = offset - output_elf_file->tls_template_offset + section->size;
            output_elf_file->tls_template_tbss_size = section->size;
        }
    }

    // Allocate memory for the output file
    output_elf_file->size = offset;
    output_elf_file->data = calloc(1, output_elf_file->size);
}

// Copy all the section data to the final positions in the ELF file.
void copy_output_sections_to_elf(OutputElfFile *output_elf_file) {
    List *sections = output_elf_file->sections_list;

    for (int i = 1; i < sections->length; i++) {
        OutputSection *section = sections->elements[i];

        if (section->type != SHT_NOBITS) {
            if (section->size && !section->data) panic("Output section data is NULL for non-zero sized section %s %p", section->name, section);
            memcpy(&output_elf_file->data[section->offset], section->data, section->size);
        }
    }
}

// Write the ELF file
void write_elf_file(OutputElfFile *output_elf_file) {
    // Copy all the section data to the final positions in the ELF file.
    copy_output_sections_to_elf(output_elf_file);

    // Write output file
    FILE *f;
    if (!strcmp(output_elf_file->filename, "-")) {
        f = stdout;
    }
    else {
        f = fopen(output_elf_file->filename, "wb");
        if (!f) { perror("Unable to open write output file"); exit(1); }
    }

    // If the output is executable, set the exec flag
    if (output_elf_file->type == ET_EXEC) {
        if (chmod(output_elf_file->filename, 0755) < 0) {
            perror("Unable to set executable permissions");
            exit(1);
        }
    }

    int written = fwrite(output_elf_file->data, 1, output_elf_file->size, f);
    if (written < 0) { perror("Unable to write to output file"); exit(1); }
    fclose(f);
}
