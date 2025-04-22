#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "list.h"
#include "ro-elf.h"
#include "rw-elf.h"

#include "wld/libs.h"
#include "wld/relocations.h"
#include "wld/symbols.h"
#include "wld/wld.h"

static const char *SUPPORTED_SECTION_NAMES[] = {
    ".text", ".data", ".rodata", ".bss"
};

static void add_bss_rw_section(RwElfFile *output);

// Go down all input files which are either object files or libraries
static List *read_input_files(List *library_paths, List *input_files) {
    List *input_elf_files = new_list(32);

    for (int i = 0; i < input_files->length; i++) {
        InputFile *input_file = input_files->elements[i];
        char *input_filename = input_file->filename;

        if (input_file->is_library) {
            char *path = search_for_library(library_paths, input_filename);
            ArchiveFile *ar_file = open_archive_file(path);
            process_library_symbols(ar_file, input_elf_files);
        }
        else {
            ElfFile *elf_file = open_elf_file(input_filename);
            process_elf_file_symbols(elf_file, 0, 0);
            append_to_list(input_elf_files, elf_file);
        }
    }

    return input_elf_files;
}

// Add some sections that are always present in output ELF file
static void create_default_sections(RwElfFile *output_elf_file) {
    add_rw_section(output_elf_file, "" , SHT_NULL, 0, 0);

    output_elf_file->section_symtab      = add_rw_section(output_elf_file, ".symtab",   SHT_SYMTAB, 0, 8);
    output_elf_file->section_strtab      = add_rw_section(output_elf_file, ".strtab",   SHT_STRTAB, 0, 1);
    output_elf_file->section_shstrtab    = add_rw_section(output_elf_file, ".shstrtab", SHT_STRTAB, 0, 1);

    output_elf_file->section_symtab->entsize = sizeof(ElfSymbol);
    add_to_rw_section(output_elf_file->section_strtab, "", 1);
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
            int sh_type = input_section->elf_section_header->sh_type;
            if (!EXECUTABLE_SECTION_TYPE(input_section->elf_section_header->sh_type)) continue;

            // Create a section, if it already exists, amend the alignment if necessary.
            RwSection *rw_section = get_rw_section(output_elf_file, name);
            if (!rw_section) {
                if (!strcmp(name, ".bss")) {
                    add_bss_rw_section(output_elf_file);
                }
                else
                    add_rw_section(output_elf_file, name, elf_section_header->sh_type, elf_section_header->sh_flags, elf_section_header->sh_addralign);
            }
            else {
                rw_section->align = MAX(rw_section->align, elf_section_header->sh_addralign);
            }
        }
    }

    create_global_offset_table(output_elf_file);
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
            if (!EXECUTABLE_SECTION_TYPE(input_section->elf_section_header->sh_type)) continue;

            // Look up the RW section. It must already exist.
            const char *section_name = &elf_file->section_header_strings[elf_section_header->sh_name];
            RwSection *rw_section = get_rw_section(output_elf_file, section_name);
            if (!rw_section) panic("Unexpected null section in output when laying out sections for %s", section_name);

            // Align the section
            int offset = ALIGN_UP(rw_section->size, rw_section->align);
            input_section->offset = offset;
            input_section->dst_section = rw_section;

            if (DEBUG) printf("File %s section %s is at offset %#08x in target section\n", elf_file->filename, input_section->name, offset);

            rw_section->size = offset + elf_section_header->sh_size;
        }
    }
}

// Unconditionally create a .bss section
static void add_bss_rw_section(RwElfFile *output) {
    int starting_alignment = 1;
    output->section_bss = add_rw_section(output, ".bss", SHT_NOBITS, SHF_ALLOC | SHF_WRITE, starting_alignment);
}

// If there are any common symbols, create a bss section and allocate values the symbols
static void add_common_symbols_to_bss(RwElfFile *output) {
    if (!common_symbols_are_present()) return;

    // A bss section is required. Check if it already exists, since e.g. symbols in glibc can also go directly to a .bss section.
    RwSection *section_bss = get_rw_section(output, ".bss");

    // Create the .bss section
    if (!section_bss) {
        add_bss_rw_section(output);
        section_bss = output->section_bss;
    }

    layout_common_symbols_in_bss_section(output->section_bss);
}

// Populate the first program segment header. This contains a page that has the start of the executable
static void make_first_program_segment_header(RwElfFile *output) {
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
    psh->p_filesz = section->size;  // Segment size in file
    psh->p_memsz = section->size;   // Segment size in memory
}

// Make all ELF program segment headers
void make_program_segment_headers(RwElfFile *output) {
    int needs_tls = 0;

    // Determine the amount of program segments
    output->elf_program_segments_count = 0;
    for (int i = 0; i < output->sections_list->length; i++) {
        RwSection *section = output->sections_list->elements[i];

        // Setup program segments if it's an executable
        if (EXECUTABLE_SECTION_TYPE(section->type)) {
            if (section->flags & SHF_TLS) {
                needs_tls = 1;

                // Include .tdata section, but not .tbss section
                if (section->type == SHT_PROGBITS)
                    section->program_segment_index = output->elf_program_segments_count++;
            }
            else {
                section->program_segment_index = output->elf_program_segments_count++;
            }
        }
    }

    int tls_program_segment_index;
    if (needs_tls)
        tls_program_segment_index = output->elf_program_segments_count++;

    // Allocate memory for the program segment headers
    output->elf_program_segments_header_size = sizeof(ElfProgramSegmentHeader) * output->elf_program_segments_count;
    output->elf_program_segment_headers = calloc(1, output->elf_program_segments_header_size);

    // Populate the null program segment header
    make_first_program_segment_header(output);

    ElfSectionHeader *elf_section_headers = output->elf_section_headers;
    ElfProgramSegmentHeader *elf_program_segment_headers = output->elf_program_segment_headers;

    // Loop over all output sections
    for (int i = 0; i < output->sections_list->length; i++) {
        RwSection *section = output->sections_list->elements[i];

        if (!EXECUTABLE_SECTION_TYPE(section->type) || section->type == SHT_NULL) continue;

        int program_segment_index = section->program_segment_index;
        if (program_segment_index >= output->elf_program_segments_count)
        panic("Bad accounting in program segments loop: %d >= %d", program_segment_index, output->elf_program_segments_count);

        make_program_segment_header(output, &output->elf_program_segment_headers[program_segment_index], section);

        ElfProgramSegmentHeader *h = &elf_program_segment_headers[program_segment_index];

        int offset = elf_section_headers[i].sh_offset;
        uint64_t vaddr = elf_section_headers[i].sh_addr;

        h->p_offset = offset;
        h->p_vaddr = vaddr;
        h->p_paddr = vaddr;
    }

    if (needs_tls) {
        ElfProgramSegmentHeader *h = &elf_program_segment_headers[tls_program_segment_index];

        h->p_type = PT_TLS;
        h->p_flags = PF_R;
        h->p_offset = output->tls_template_offset;
        h->p_filesz = output->tls_template_tdata_size;
        h->p_memsz = output->tls_template_tdata_size + output->tls_template_tbss_size;
        h->p_vaddr = output->tls_template_virt_address;
        h->p_paddr = output->tls_template_virt_address;
        h->p_align = 0x1000;
    }
}

// Assign final values to all symbols
static void make_symbol_values(List *input_elf_files, RwElfFile *output_elf_file) {
    if (DEBUG) printf("\nGlobal symbols:\n");

    // Global symbols
    make_symbol_values_from_symbol_table(output_elf_file, output_elf_file->executable_virt_address, global_symbol_table);

    // Local symbols
    for (int i = 0; i < input_elf_files->length; i++) {
        ElfFile *elf_file = input_elf_files->elements[i];
        if (DEBUG) printf("\nLocal symbols for %s:\n", elf_file->filename);
        SymbolTable *local_symbol_table = get_local_symbol_table(elf_file);
        make_symbol_values_from_symbol_table(output_elf_file, output_elf_file->executable_virt_address, local_symbol_table);
    }
}

static void make_array_symbol_values(RwElfFile *output_elf_file) {
    for (int i = 0; i < output_elf_file->sections_list->length; i++) {
        RwSection *section = output_elf_file->sections_list->elements[i];

        #define SET_START_END(start_symbol, end_symbol) { \
            must_get_global_defined_symbol(start_symbol)->dst_value = output_elf_file->executable_virt_address + section->offset; \
            must_get_global_defined_symbol(end_symbol)->dst_value = output_elf_file->executable_virt_address + section->offset + section->size; \
        }

        // Fragile: if the section is empty, it won't have a value, and the start/end symbols will both be zero.
        // This problem will go away when linker scripts are implemented since the start/end will end up somewhere
        // in the data segment.
        if (!strcmp(section->name, ".preinit_array")) {
            SET_START_END(PREINIT_ARRAY_START_SYMBOL_NAME, PREINIT_ARRAY_END_SYMBOL_NAME);
        }

        if (!strcmp(section->name, ".init_array")) {
            SET_START_END(INIT_ARRAY_START_SYMBOL_NAME, INIT_ARRAY_END_SYMBOL_NAME);
        }

        if (!strcmp(section->name, ".fini_array")) {
            SET_START_END(FINI_ARRAY_START_SYMBOL_NAME, FINI_ARRAY_END_SYMBOL_NAME);
        }
    }
}

// Set the executable entrypoint
static void set_entrypoint(RwElfFile *output_elf_file) {
    Symbol *symbol = get_defined_symbol(global_symbol_table, ENTRYPOINT_SYMBOL);
    if (!symbol) error("Missing %s symbol", ENTRYPOINT_SYMBOL);
    output_elf_file->entrypoint = symbol->dst_value;
}

// Find the virtual address and size of the TLS template, if present
static void prepare_tls_template(RwElfFile *output_elf_file) {
    RwSection *tdata_section = get_rw_section(output_elf_file, ".tdata");
    RwSection *tbss_section = get_rw_section(output_elf_file, ".tbss");

    // If there are both .tdata and .bss sections, ensure they are are consecutive
    if (tdata_section && tbss_section && tdata_section->index != tbss_section->index - 1)
        panic(".tdata and .tss sections aren't consecutive: %d != %d - 1", tdata_section->index, tbss_section->index);

    if (tdata_section && !tbss_section) {
        output_elf_file->tls_template_size = tdata_section->size;
        output_elf_file->tls_template_virt_address = output_elf_file->executable_virt_address + tdata_section->offset;
    }
    else if (!tdata_section && tbss_section) {
        output_elf_file->tls_template_size = tbss_section->size;
        output_elf_file->tls_template_virt_address = output_elf_file->executable_virt_address + tbss_section->offset;
    }
    else if (tdata_section && tbss_section) {
        output_elf_file->tls_template_size = tbss_section->offset - tdata_section->offset + tbss_section->size;
        output_elf_file->tls_template_virt_address = output_elf_file->executable_virt_address + tdata_section->offset;
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
            if (!EXECUTABLE_SECTION_TYPE(input_section->elf_section_header->sh_type)) continue;

            const char *section_name = &input_elf_file->section_header_strings[input_elf_section_header->sh_name];
            RwSection *rw_section = get_rw_section(output_elf_file, section_name);
            if (!rw_section) panic("Unexpected null section in output when copying sections to output");

            // Allocate memory if not already done in a previous loop
            if (!rw_section->data) rw_section->data = calloc(1, rw_section->size);

            // Load the section data. It may already have been loaded and modified by relocations.
            load_section(input_elf_file, input_section);
            memcpy(rw_section->data + input_section->offset, input_section->data, input_section->elf_section_header->sh_size);
        }
    }
}

void run(List *library_paths, List *input_files, const char *output_filename) {
    // Setup symbol tables
    init_symbols();

    // Read input file
    List *input_elf_files = read_input_files(library_paths, input_files);

    // At this point all symbols should be defined. Ensure this is the case.
    finalize_symbols();

    // Relax instructions where possible and determine which symbols need to be in the GOT
    apply_relocations(input_elf_files, NULL, RELOCATION_PHASE_SCAN);

    // Create output file
    RwElfFile *output_elf_file = new_rw_elf_file(output_filename, ET_EXEC);
    output_elf_file->executable_virt_address = EXECUTABLE_VIRTUAL_ADDRESS;

    // Create sections in the output file
    create_output_file_sections(input_elf_files, output_elf_file);

    // Determine layout of the input sections in the output sections
    layout_output_sections(input_elf_files, output_elf_file);

    // If there are any common symbols, create a bss section and allocate values the symbols
    add_common_symbols_to_bss(output_elf_file);

    // Rearrange sections list
    make_section_indexes(output_elf_file);

    // Add the symbols to the ELF symbol table
    make_elf_symbols(output_elf_file);

    // Make all ELF section headers
    make_rw_section_headers(output_elf_file);

    // Layout the sections & allocate memory for the output
    layout_rw_elf_sections(output_elf_file);

    // Make the program segment headers
    make_program_segment_headers(output_elf_file);

    // Assign final values to all symbols
    make_symbol_values(input_elf_files, output_elf_file);

    // Assign values to array start/end section built-in symbols
    make_array_symbol_values(output_elf_file);

    // Update the GOT (if there is one)
    update_got_symbol_values(output_elf_file);

    // Set the symbol's value and section indexes
    update_elf_symbols(output_elf_file);

    // Set the executable entrypoint
    set_entrypoint(output_elf_file);

    // Make the ELF headers, program segment headers and section headers
    make_elf_headers(output_elf_file);

    prepare_tls_template(output_elf_file);

    // Copy the memory for all program sections in the input files to the output file
    copy_input_elf_sections_to_output(input_elf_files, output_elf_file);

    // Copy all the section data to the final positions in the ELF file.
    copy_rw_sections_to_elf(output_elf_file);

    // Write relocated symbol values to the output ELF file
    apply_relocations(input_elf_files, output_elf_file, RELOCATION_PHASE_APPLY);

    // Write the ELF file
    write_elf_file(output_elf_file);
}
