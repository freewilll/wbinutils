#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "error.h"
#include "list.h"
#include "ro-elf.h"
#include "rw-elf.h"
#include "wld.h"

// Load all files into memoryfiles
static List *read_input_files(List *input_filenames) {
    List *input_elf_files = new_list(32);

    for (int i = 0; i < input_filenames->length; i++) {
        char *input_filename = input_filenames->elements[i];
        InputElfFile *elf_file = read_elf_file(input_filename);
        append_to_list(input_elf_files, elf_file);
    }

    return input_elf_files;
}

// Add some sections that are always present in output ELF file
static void create_default_sections(RwElfFile *output_elf_file) {
    add_rw_section(output_elf_file, "" , SHT_NULL, 0, 0);
    output_elf_file->shstrtab = add_rw_section(output_elf_file, ".shstrtab", SHT_STRTAB, 0, 1);
}

// Rearrange sections list so that .symtab, .strtab and .shstrtab are last.
// Then set the index.
static void make_section_indexes(RwElfFile *output_elf_file) {
    List *sections_list = output_elf_file->sections_list;

    List *new_sections_list = new_list(sections_list->length);
    List *selected_sections_list = new_list(3);

    for (int i = 0; i < sections_list->length; i++) {
        RwSection *section = sections_list->elements[i];

        if (!strcmp(section->name, ".symtab") || !strcmp(section->name, ".strtab") || !strcmp(section->name, ".shstrtab"))
            append_to_list(selected_sections_list, section);
        else
            append_to_list(new_sections_list, section);
    }

    free_list(sections_list);

    int non_selected_sections_length = new_sections_list->length;

    // Set the index on the first sections
    for (int i = 0; i < non_selected_sections_length; i++) {
        RwSection *section = new_sections_list->elements[i];
        section->index = i;
    }

    // Append the remaining sections
    for (int i = 0; i < selected_sections_list->length; i++) {
        RwSection *section = selected_sections_list->elements[i];
        section->index = non_selected_sections_length + i;
        append_to_list(new_sections_list, section);
    }

    free_list(selected_sections_list);

    output_elf_file->sections_list = new_sections_list;
}

// Loop over all sections in the input files and create the target sections in the output file.
static void create_output_file_sections(List *input_elf_files, RwElfFile *output_elf_file) {
    create_default_sections(output_elf_file);

    // Loop over all files
    for (int i = 0; i < input_elf_files->length; i++) {
        InputElfFile *elf_file = input_elf_files->elements[i];

        // Loop over all sections
        for (int j = 0; j < elf_file->section_list->length; j++) {
            InputSection *input_section  = (InputSection *) elf_file->section_list->elements[j];
            ElfSectionHeader *elf_section_header = input_section->elf_section_header;
            const char *name = &elf_file->section_header_strings[elf_section_header->sh_name];

            // Only include sections that have program data
            if (input_section->elf_section_header->sh_type != SHT_PROGBITS) continue;

            // Create a section, if it already exists, amend the alignment if necessary.
            RwSection *rw_section = get_rw_section(output_elf_file, name);
            if (!rw_section) {
                rw_section = add_rw_section(output_elf_file, name, elf_section_header->sh_type, elf_section_header->sh_flags, elf_section_header->sh_addralign);
            }
            else {
                rw_section->align = MAX(rw_section->align, elf_section_header->sh_addralign);
            }
        }
    }

    make_section_indexes(output_elf_file);
}

// Determine the layout of the input sections in the output sections, aligning
// them as necessary
static void layout_output_sections(List *input_elf_files, RwElfFile *output_elf_file) {
    // Loop over all files
    for (int i = 0; i < input_elf_files->length; i++) {
        InputElfFile *elf_file = input_elf_files->elements[i];

        // Loop over all sections
        for (int j = 0; j < elf_file->section_list->length; j++) {
            InputSection *input_section  = (InputSection *) elf_file->section_list->elements[j];
            ElfSectionHeader *elf_section_header = input_section->elf_section_header;

            // Only include sections that have program data
            if (elf_section_header->sh_type != SHT_PROGBITS) continue;

            // Look up the RW section. It must already exist.
            const char *section_name = &elf_file->section_header_strings[elf_section_header->sh_name];
            RwSection *rw_section = get_rw_section(output_elf_file, section_name);
            if (!rw_section) panic("Unexpected null section in output");

            // Align the section
            int offset = ALIGN_UP(rw_section->size, rw_section->align);
            input_section->offset = offset;
            rw_section->size = offset + elf_section_header->sh_size;
        }
    }
}

// Populate the null program segment header
static void make_null_program_segment_header(RwElfFile *output) {
    ElfProgramSegmentHeader *h = &output->elf_program_segment_headers[0];

    int size = output->elf_section_headers_offset + output->elf_section_headers_size;

    h->p_type   = PT_LOAD;                          // Loadable
    h->p_flags  = PF_R;                             // Read only
    h->p_vaddr  = output->executable_virt_address;  // Start of the executable
    h->p_paddr  = output->executable_virt_address;
    h->p_filesz = size;
    h->p_memsz  = size;
    h->p_align  = 0x1000;                           // Align on page boundaries
}

// Make an ELF program segment header
static void make_program_segment_header(RwElfFile *output_elf_file, ElfProgramSegmentHeader *psh, RwSection *section) {
    psh->p_flags = PF_R;

    if (section->flags & SHF_WRITE) psh->p_flags |= PF_W;
    if (section->flags & SHF_EXECINSTR) psh->p_flags |= PF_X;

    psh->p_type = PT_LOAD;          // Segment type
    psh->p_vaddr = 0;               // Segment virtual address
    psh->p_paddr = 0;               // Segment physical address
    psh->p_filesz = section->size;  // Segment size in file
    psh->p_memsz = section->size;   // Segment size in memory
    psh->p_align = 0x1000;		    // Segments are aligned on page boundaries
}

// Make all ELF program segment headers
// There is a one-to-one mapping between the sections of type SHT_PROGBITS and the segments.
static void make_program_segment_headers(RwElfFile *output) {
    // Count the amount of sections. // The 0th segment is the null segment.
    output->elf_program_segments_count = 1;

    for (int i = 0; i < output->sections_list->length; i++) {
        RwSection *section = output->sections_list->elements[i];
        if (section->type != SHT_PROGBITS) continue;
        output->elf_program_segments_count += 1;
    }

    // Allocate memory for the program segment headers
    output->elf_program_segments_header_size = sizeof(ElfProgramSegmentHeader) * output->elf_program_segments_count;
    output->elf_program_segment_headers = calloc(1, output->elf_program_segments_header_size);

    // Populate the program segment headers
    int count = 1;
    for (int i = 0; i < output->sections_list->length; i++) {
        RwSection *section = output->sections_list->elements[i];
        if (section->type != SHT_PROGBITS) continue;

        make_program_segment_header(output, &output->elf_program_segment_headers[count], section);
        count += 1;
    }
}

// Make an ELF section header
static void make_section_header(RwElfFile *output_elf_file, ElfSectionHeader *sh, RwSection *section) {
    sh->sh_name      = add_to_rw_section(output_elf_file->shstrtab, (char *) section->name, strlen(section->name) + 1);
    sh->sh_type      = section->type;
    sh->sh_flags     = section->flags;
    sh->sh_offset    = section->offset;
    sh->sh_size      = section->size;
    sh->sh_addralign = section->align;
}

// Make all output section headers
static void make_section_headers(RwElfFile *output_elf_file) {
    // Allocat ememory
    output_elf_file->elf_section_headers_size = sizeof(ElfSectionHeader) * output_elf_file->sections_list->length;
    output_elf_file->elf_section_headers = calloc(1, output_elf_file->elf_section_headers_size);

    // Loop over all headers
    for (int i = 0; i < output_elf_file->sections_list->length; i++) {
        RwSection *section = output_elf_file->sections_list->elements[i];
        make_section_header(output_elf_file, &output_elf_file->elf_section_headers[i], section);
    }
}

// Given the sizes and alignments of all output sections, determine the offsets in the final ELF file.
static int layout_elf_sections(RwElfFile *output_elf_file) {
    ElfSectionHeader *elf_section_headers = output_elf_file->elf_section_headers;
    ElfProgramSegmentHeader *elf_program_segment_headers = output_elf_file->elf_program_segment_headers;

    // Determine section offsets
    // Align start of the sections on a page boundary after the ELF, program segments and section headers
    int offset =
        sizeof(ElfHeader) +                                     // Elf header
        output_elf_file->elf_program_segments_header_size +     // Program segments headers
        output_elf_file->elf_section_headers_size;              // Section headers

    // Loop over all output sections
    int program_segment_index = 0;
    for (int i = 0; i < output_elf_file->sections_list->length; i++) {
        RwSection *section = output_elf_file->sections_list->elements[i];

        // Align program sections to page boundaries
        if (section->type == SHT_PROGBITS) offset = ALIGN_UP(offset, 0x1000);

        // Determine executable address of the section
        uint64_t vaddr = output_elf_file->executable_virt_address + offset;

        section->offset = offset;
        elf_section_headers[i].sh_offset = offset;
        elf_section_headers[i].sh_addr = vaddr;

        if (section->type == SHT_PROGBITS) {
            program_segment_index++;

            ElfProgramSegmentHeader *h = &elf_program_segment_headers[program_segment_index];

            h->p_offset = offset;
            h->p_vaddr = vaddr;
            h->p_paddr = vaddr;
        }

        offset += section->size;
    }

    return offset;
}

// Make the ELF header
static void make_elf_header(ElfHeader *elf_header, RwElfFile *output) {
    uint64_t entrypoint = 0x401000; // TODO look up from symbol table

    // ELF header
    elf_header->ei_magic0 = 0x7f;                                  // Magic
    elf_header->ei_magic1 = 'E';
    elf_header->ei_magic2 = 'L';
    elf_header->ei_magic3 = 'F';
    elf_header->ei_class    = ELF_CLASS_64;                        // 64-bit
    elf_header->ei_data     = ELF_DATA_2_LSB;                      // LSB
    elf_header->ei_version  = 1;                                   // Original ELF version
    elf_header->ei_osabi    = ELF_OSABI_NONE;                      // Unix System V
    elf_header->e_type      = ET_EXEC;                             // Executable file
    elf_header->e_machine   = E_MACHINE_TYPE_X86_64;               // x86-64
    elf_header->e_version   = EV_CURRENT;                          // EV_CURRENT Current version of ELF
    elf_header->e_entry     = entrypoint;                          // Program entrypoint
    elf_header->e_phoff     = output->elf_program_segments_offset; // Offset to program header table
    elf_header->e_shoff     = output->elf_section_headers_offset;  // Offset to section header table
    elf_header->e_ehsize    = sizeof(ElfHeader);                   // The size of this header, 0x40 for 64-bit
    elf_header->e_phentsize = sizeof(ElfProgramSegmentHeader);     // The size of the program header
    elf_header->e_phnum     = output->elf_program_segments_count;  // Number of program header entries
    elf_header->e_shentsize = sizeof(ElfSectionHeader);            // The size of the section header
    elf_header->e_shnum     = output->sections_list->length;       // Number of section header entries
    elf_header->e_shstrndx  = output->shstrtab->index;             // The section header string table index
}

// Copy the memory for all program sections in the input files to the output file
static void copy_input_elf_sections_to_output(List *input_elf_files, RwElfFile *output_elf_file) {
    // Loop over all files
    for (int i = 0; i < input_elf_files->length; i++) {
        InputElfFile *input_elf_file = input_elf_files->elements[i];

        // Loop over all sections
        for (int j = 0; j < input_elf_file->section_list->length; j++) {
            InputSection *input_section  = (InputSection *) input_elf_file->section_list->elements[j];
            ElfSectionHeader *input_elf_section_header = input_section->elf_section_header;

            // Only include sections that have program data
            if (input_section->elf_section_header->sh_type != SHT_PROGBITS) continue;

            const char *section_name = &input_elf_file->section_header_strings[input_elf_section_header->sh_name];
            RwSection *rw_section = get_rw_section(output_elf_file, section_name);
            if (!rw_section) panic("Unexpected null section in output");

            // Allocate memory if not already done in a previous loop
            if (!rw_section->data) rw_section->data = calloc(1, rw_section->size);

            // Copy the section data
            memcpy(rw_section->data + input_section->offset, input_elf_file->data + input_elf_section_header->sh_offset, input_elf_section_header->sh_size);
        }
    }
}

// Copy all the section data to the final positions in the ELF file.
static void copy_sections_to_elf(RwElfFile *output_elf_file, char *program) {
    List *sections = output_elf_file->sections_list;

    for (int i = 0; i < sections->length; i++) {
        RwSection *section = sections->elements[i];

        // All sections have data other than .bss
        if (strcmp(section->name, ".bss")) {
            memcpy(&program[section->offset], section->data, section->size);
        }
    }
}

// Write the ELF file
void write_elf_file(const char *filename, const void *program, int size) {
    // Write output file
    FILE *f;
    if (!strcmp(filename, "-")) {
        f = stdout;
    }
    else {
        f = fopen(filename, "wb");
        if (!f) {
            perror("Unable to open write output file");
            exit(1);
        }

        if (chmod(filename, 0755) < 0) {
            perror("Unable to set executable permissions");
            exit(1);
        }
    }

    int written = fwrite(program, 1, size, f);
    if (written < 0) { perror("Unable to write to output file"); exit(1); }
    fclose(f);
}

void run(List *input_filenames, const char *output_filename) {
    // Read input file
    List *input_elf_files = read_input_files(input_filenames);

    // Create output file
    RwElfFile *output_elf_file = new_rw_elf_file(output_filename, EXECUTABLE_VIRTUAL_ADDRESS);

    // Create sections in the output file
    create_output_file_sections(input_elf_files, output_elf_file);

    // Determine layuout of the input sections in the output sections
    layout_output_sections(input_elf_files, output_elf_file);

    // Make all ELF program segment headers
    make_program_segment_headers(output_elf_file);

    // Make all ELF section headers
    make_section_headers(output_elf_file);

    // Populate the null program segment header
    make_null_program_segment_header(output_elf_file);

    // Layout the sections
    int size = layout_elf_sections(output_elf_file);

    // Allocate memory for the output program
    char *program = calloc(1, size);

    // Layout the first page as follows:
    // - ELF header
    // - Program segment headers
    // - Segment headers
    output_elf_file->elf_program_segments_offset  = sizeof(ElfSectionHeader);
    output_elf_file->elf_section_headers_offset  = output_elf_file->elf_program_segments_offset + output_elf_file->elf_program_segments_header_size;

    // Make/copy the ELF, program segment headers and section headers
    make_elf_header((ElfHeader *) program, output_elf_file);
    memcpy(program + output_elf_file->elf_program_segments_offset, output_elf_file->elf_program_segment_headers, output_elf_file->elf_program_segments_header_size);
    memcpy(program + output_elf_file->elf_section_headers_offset, output_elf_file->elf_section_headers, output_elf_file->elf_section_headers_size);

    // Copy the memory for all program sections in the input files to the output file
    copy_input_elf_sections_to_output(input_elf_files, output_elf_file);

    // Copy all the section data to the final positions in the ELF file.
    copy_sections_to_elf(output_elf_file, program);

    // Write the ELF file
    write_elf_file(output_filename, program, size);
}
