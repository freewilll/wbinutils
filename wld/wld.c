#include <stdlib.h>

#include "error.h"
#include "list.h"
#include "ro-elf.h"
#include "rw-elf.h"

#include "wld/libs.h"
#include "wld/wld.h"

// Go down all input files which are either object files or libraries
static List *read_input_files(List *library_paths, List *input_files) {
    List *input_elf_files = new_list(32);

    for (int i = 0; i < input_files->length; i++) {
        InputFile *input_file = input_files->elements[i];
        char *input_filename = input_file->filename;

        if (input_file->is_library) {
            char *path = search_for_library(library_paths, input_filename);
            ArchiveFile *ar_file = open_archive_file(path);
        }
        else {
            ElfFile *elf_file = open_elf_file(input_filename);
            append_to_list(input_elf_files, elf_file);
        }
    }

    return input_elf_files;
}

// Add some sections that are always present in output ELF file
static void create_default_sections(RwElfFile *output_elf_file) {
    add_rw_section(output_elf_file, "" , SHT_NULL, 0, 0);
    output_elf_file->section_shstrtab = add_rw_section(output_elf_file, ".shstrtab", SHT_STRTAB, 0, 1);
}

// Loop over all sections in the input files and create the target sections in the output file.
static void create_output_file_sections(List *input_elf_files, RwElfFile *output_elf_file) {
    create_default_sections(output_elf_file);

    // Loop over all files
    for (int i = 0; i < input_elf_files->length; i++) {
        ElfFile *elf_file = input_elf_files->elements[i];

        // Loop over all sections
        for (int j = 0; j < elf_file->section_list->length; j++) {
            Section *input_section  = (Section *) elf_file->section_list->elements[j];
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
        ElfFile *elf_file = input_elf_files->elements[i];

        // Loop over all sections
        for (int j = 0; j < elf_file->section_list->length; j++) {
            Section *input_section  = (Section *) elf_file->section_list->elements[j];
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

// Copy the memory for all program sections in the input files to the output file
static void copy_input_elf_sections_to_output(List *input_elf_files, RwElfFile *output_elf_file) {
    // Loop over all files
    for (int i = 0; i < input_elf_files->length; i++) {
        ElfFile *input_elf_file = input_elf_files->elements[i];

        // Loop over all sections
        for (int j = 0; j < input_elf_file->section_list->length; j++) {
            Section *input_section  = (Section *) input_elf_file->section_list->elements[j];
            ElfSectionHeader *input_elf_section_header = input_section->elf_section_header;

            // Only include sections that have program data
            if (input_section->elf_section_header->sh_type != SHT_PROGBITS) continue;

            const char *section_name = &input_elf_file->section_header_strings[input_elf_section_header->sh_name];
            RwSection *rw_section = get_rw_section(output_elf_file, section_name);
            if (!rw_section) panic("Unexpected null section in output");

            // Allocate memory if not already done in a previous loop
            if (!rw_section->data) rw_section->data = calloc(1, rw_section->size);

            // Load the section data
            load_section_into_buffer(input_elf_file, input_section->index, rw_section->data + input_section->offset);
        }
    }
}

void run(List *library_paths, List *input_files, const char *output_filename) {
    // Read input file
    List *input_elf_files = read_input_files(library_paths, input_files);

    // Create output file
    RwElfFile *output_elf_file = new_rw_elf_file(output_filename, ET_EXEC);
    output_elf_file->executable_virt_address = EXECUTABLE_VIRTUAL_ADDRESS;
    output_elf_file->entrypoint = 0x401000; // TODO look up from symbol table

    // Create sections in the output file
    create_output_file_sections(input_elf_files, output_elf_file);

    // Determine layout of the input sections in the output sections
    layout_output_sections(input_elf_files, output_elf_file);

    // Make all ELF program segment headers
    make_program_segment_headers(output_elf_file);

    // Make all ELF section headers
    make_rw_section_headers(output_elf_file);

    // Populate the null program segment header
    make_null_program_segment_header(output_elf_file);

    // Layout the sections & allocate memory for the output
    layout_rw_elf_sections(output_elf_file);

    // Make the ELF headers, program segment headers and section headers
    make_elf_headers(output_elf_file);

    // Copy the memory for all program sections in the input files to the output file
    copy_input_elf_sections_to_output(input_elf_files, output_elf_file);

    // Copy all the section data to the final positions in the ELF file.
    copy_rw_sections_to_elf(output_elf_file);

    // Write the ELF file
    write_elf_file(output_elf_file);
}
