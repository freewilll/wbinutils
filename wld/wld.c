#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "list.h"
#include "ro-elf.h"
#include "rw-elf.h"
#include "strmap-ordered.h"

#include "wld/layout.h"
#include "wld/lexer.h"
#include "wld/parser.h"
#include "wld/libs.h"
#include "wld/relocations.h"
#include "wld/script.h"
#include "wld/symbols.h"
#include "wld/utils.h"
#include "wld/wld.h"

static char *entrypoint_symbol_name;

static const char *SECTION_TYPE_NAMES[] = {
    "NULL",
    "PROGBITS",
    "SYMTAB",
    "STRTAB",
    "RELA",
    "HASH",
    "DYNAMIC",
    "NOTE",
    "NOBITS",
    "REL",
    "SHLIB",
    "DYNSYM",
    "UNKNOWN",
    "UNKNOWN",
    "INIT_ARRAY",
    "FINI_ARRAY",
    "PREINIT_ARRAY",
    "GROUP",
    "SYMTAB_SHNDX",
    "NUM",
};

static const char *PROGRAM_SEGMENT_TYPE_NAMES[] = { "NULL", "LOAD", "DYNAMIC", "INTERP", "NOTE", "SHLIB", "PHDR", "TLS", "NUM" };

static RwElfFile *init_output_elf_file(const char *output_filename) {
    RwElfFile *result = new_rw_elf_file(output_filename, ET_EXEC);
    result->extra_sections = new_strmap_ordered();
    result->ifunc_symbols = new_list(0);
    result->global_symbols_in_use  = new_strmap();

    return result;
}

// Get an extra section. Returns null if it doesn't exist
Section *get_extra_section(RwElfFile *output_elf_file, char *name) {
    Section *section = strmap_ordered_get(output_elf_file->extra_sections, name);
    return section;
}

// Create an extra section. It must not already exist.
Section *create_extra_section(RwElfFile *output_elf_file, char *name, uint32_t type, uint64_t flags, uint64_t align) {
    if (strmap_ordered_get(output_elf_file->extra_sections, name))
        panic("Extra section %s already exists", name);

    Section *extra_section = calloc(1, sizeof(Section));
    extra_section->name = strdup(name);
    extra_section->type = type;
    extra_section->flags = flags;
    extra_section->align = align;
    strmap_ordered_put(output_elf_file->extra_sections, strdup(name), extra_section);

    return extra_section;
}

// Static libraries subh as libm.a can consist of a linker script that looks something like:
// /* GNU ld script
// */
// OUTPUT_FORMAT(elf64-x86-64)
// GROUP ( /usr/lib/x86_64-linux-gnu/libm-2.31.a /usr/lib/x86_64-linux-gnu/libmvec.a )
// Parse the linker script and process all libraries in the GROUP()
static void run_archive_file_linker_script(char *path, List *input_elf_files) {
    init_lexer(path);
    List *linker_script = parse();

    // Loop over the group
    for (int i = 0; i < linker_script->length; i++) {
        ScriptCommand *command = linker_script->elements[i];
        if (command->type != CMD_GROUP) continue;

        // Loop over the group until no more objects are added
        while (1) {
            int objects_added = 0;

            List *filenames = command->group.filenames;
            for (int j = 0; j < filenames->length; j++) {
                char *path = filenames->elements[j];

                ArchiveFile *ar_file = open_archive_file(path);
                objects_added += process_library_symbols(ar_file, input_elf_files);
            }

            if (!objects_added) break;
        }
    }
}

// Go down all input files which are either object files or libraries
static List *read_input_files(List *library_paths, List *input_files) {
    List *input_elf_files = new_list(32);

    for (int i = 0; i < input_files->length; i++) {
        InputFile *input_file = input_files->elements[i];
        char *input_filename = input_file->filename;
        if (DEBUG_SYMBOL_RESOLUTION) printf("Examining file %s\n", input_filename);

        if (input_file->is_library) {
            char *path = search_for_library(library_paths, input_filename);

            if (is_gnu_linker_script_file(path)) {
                run_archive_file_linker_script(path, input_elf_files);
            }
            else {
                ArchiveFile *ar_file = open_archive_file(path);
                process_library_symbols(ar_file, input_elf_files);
            }
        }
        else {
            ElfFile *elf_file = open_elf_file(input_filename);
            process_elf_file_symbols(elf_file, 0, 0);
            append_to_list(input_elf_files, elf_file);
        }
    }

    return input_elf_files;
}

// Go through the linker script and set the entrypoint symbol
static void set_entrypoint_symbol(RwElfFile *output_elf_file, List *input_elf_files) {
    for (int i = 0; i < output_elf_file->linker_script->length; i++) {
        ScriptCommand *script_command = output_elf_file->linker_script->elements[i];

        if (script_command->type == CMD_ENTRY)
            entrypoint_symbol_name = script_command->entry.symbol;
    }
}

void allocate_elf_output_memory(RwElfFile *output_elf_file) {
    output_elf_file->data = calloc(1, output_elf_file->size);
}

// Populate the first program segment header. This contains a page that has the start of the executable.
static void make_first_program_segment_header(RwElfFile *output) {
    // The lowest address is in the first program segment header.
    // Use this address to find an address low enough to hold the ELF headers and round it down to a page.
    ElfProgramSegmentHeader *first_program_segment = output->program_segments_list->elements[0];
    uint64_t address = first_program_segment->p_paddr - headers_size(output);
    address = ALIGN_DOWN(address, 0x1000);

    int size = output->elf_section_headers_offset + output->elf_section_headers_size;

    ElfProgramSegmentHeader *h = &output->elf_program_segment_headers[0];
    h->p_type   = PT_LOAD;  // Loadable
    h->p_flags  = PF_R;     // Read only
    h->p_vaddr  = address;
    h->p_paddr  = address;
    h->p_filesz = size;
    h->p_memsz  = size;
    h->p_align  = 0x1000;   // Align on page boundaries
}

// Print sections in a format similar to readelf's output
void dump_sections(RwElfFile *output_elf_file) {
    printf("Sections:\n");
    printf("[Nr] Name                          Type            Address          Off      Size     ES Flg Lk Inf Al\n");

    for (int i = 0; i < output_elf_file->sections_list->length; i++) {
        RwSection *section = output_elf_file->sections_list->elements[i];

        char flags[10] = {0};
        char *p = flags;
        if (section->flags & SHF_WRITE)     *p++ = 'W';
        if (section->flags & SHF_ALLOC)     *p++ = 'A';
        if (section->flags & SHF_EXECINSTR) *p++ = 'X';
        if (section->flags & SHF_MERGE)     *p++ = 'M';
        if (section->flags & SHF_STRINGS)   *p++ = 'S';
        if (section->flags & SHF_INFO_LINK) *p++ = 'I';
        if (section->flags & SHF_GROUP)     *p++ = 'G';
        if (section->flags & SHF_TLS)       *p++ = 'T';
        *p = '\0';

        printf("%4d %-29s %-14s  %016x %08x %08x %02lx %-3s %2d %2d %3d\n",
            i, section->name, SECTION_TYPE_NAMES[section->type], section->address, section->offset,
            section->size, section->entsize, flags, section->link, section->info, section->align);
    }
}

// Print sections in a format similar to readelf's output
void dump_program_segments(RwElfFile *output_elf_file) {
    printf("Program Segments:\n");
    printf("[Nr] Type           Offset   VirtAddr         PhysAddr         FileSiz  MemSiz   Flg Align\n");

    for (int i = 0; i < output_elf_file->program_segments_list->length; i++) {
        ElfProgramSegmentHeader *program_segment = output_elf_file->program_segments_list->elements[i];

        printf("%4d %-14s %08lx %016lx %016lx %08lx %08lx %c%c%c %06lx\n",
            i, PROGRAM_SEGMENT_TYPE_NAMES[program_segment->p_type],
            program_segment->p_offset, program_segment->p_vaddr, program_segment->p_vaddr,
            program_segment->p_filesz, program_segment->p_memsz,
            (program_segment->p_flags & PF_R) ? 'R' : ' ',
            (program_segment->p_flags & PF_W) ? 'W' : ' ',
            (program_segment->p_flags & PF_X) ? 'E' : ' ',
            program_segment->p_align
        );
    }
}

// Make the ELF program segment headers. Include the special TLS section if required.
static void make_elf_program_segment_headers(RwElfFile *output_elf_file) {
    int needs_tls = 0;

    for (int i = 0; i < output_elf_file->sections_list->length; i++) {
        RwSection *section = output_elf_file->sections_list->elements[i];
        if (section->flags & SHF_TLS) needs_tls = 1;
    }

    if (needs_tls) {
        ElfProgramSegmentHeader *tls_program_segment = calloc(1, sizeof(ElfProgramSegmentHeader));

        tls_program_segment->p_type = PT_TLS;
        tls_program_segment->p_flags = PF_R;
        tls_program_segment->p_offset = output_elf_file->tls_template_offset;
        tls_program_segment->p_filesz = output_elf_file->tls_template_tdata_size;
        tls_program_segment->p_memsz = output_elf_file->tls_template_tdata_size + output_elf_file->tls_template_tbss_size;
        tls_program_segment->p_vaddr = output_elf_file->tls_template_address;
        tls_program_segment->p_paddr = output_elf_file->tls_template_address;
        tls_program_segment->p_align = 8;

        append_to_list(output_elf_file->program_segments_list, tls_program_segment);
    }

    // Allocate memory for the program segment headers
    output_elf_file->elf_program_segments_count = output_elf_file->program_segments_list->length + 1;
    output_elf_file->elf_program_segments_header_size = sizeof(ElfProgramSegmentHeader) * output_elf_file->elf_program_segments_count;
    output_elf_file->elf_program_segment_headers = calloc(1, output_elf_file->elf_program_segments_header_size);

    output_elf_file->elf_program_segments_offset  = sizeof(ElfSectionHeader);
    output_elf_file->elf_section_headers_offset  = output_elf_file->elf_program_segments_offset + output_elf_file->elf_program_segments_header_size;

    // Populate the null program segment header
    make_first_program_segment_header(output_elf_file);

    for (int i = 0; i < output_elf_file->program_segments_list->length; i++) {
        ElfProgramSegmentHeader *program_segment = output_elf_file->program_segments_list->elements[i];
        memcpy(&output_elf_file->elf_program_segment_headers[i + 1], program_segment, sizeof(ElfProgramSegmentHeader));
    }

    if (DEBUG_LAYOUT) dump_program_segments(output_elf_file);

    // Copy program headers
    memcpy(
        output_elf_file->data + output_elf_file->elf_program_segments_offset,
        output_elf_file->elf_program_segment_headers,
        output_elf_file->elf_program_segments_header_size);
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

// Process IFUNCs and create .got.plt, .iplt and .rela.iplt if required
static void process_ifuncs(RwElfFile *output_elf_file, List *input_elf_files) {
    // Global symbols
    process_ifuncs_from_symbol_table(output_elf_file, global_symbol_table);

    // Local symbols
    for (int i = 0; i < input_elf_files->length; i++) {
        ElfFile *elf_file = input_elf_files->elements[i];
        if (DEBUG_RELOCATIONS) printf("\nLocal symbols for %s:\n", elf_file->filename);
        SymbolTable *local_symbol_table = get_local_symbol_table(elf_file);
        process_ifuncs_from_symbol_table(output_elf_file, local_symbol_table);
    }
}

// If there are any common symbols, create a bss section and allocate values the symbols
static void add_common_symbols_to_bss(RwElfFile *output) {
    if (!common_symbols_are_present()) return;

    // Create the .bss section
    if (!output->section_bss)
        output->section_bss = add_rw_section(output, ".bss", SHT_NOBITS, SHF_ALLOC | SHF_WRITE, 0);

    layout_common_symbols_in_bss_section(output->section_bss);
}

// Assign final values to all symbols
static void make_symbol_values(RwElfFile *output_elf_file, List *input_elf_files) {
    if (DEBUG_RELOCATIONS) printf("\nGlobal symbols:\n");

    // Global symbols
    make_symbol_values_from_symbol_table(output_elf_file, global_symbol_table);

    // Local symbols
    for (int i = 0; i < input_elf_files->length; i++) {
        ElfFile *elf_file = input_elf_files->elements[i];
        if (DEBUG_RELOCATIONS) printf("\nLocal symbols for %s:\n", elf_file->filename);
        SymbolTable *local_symbol_table = get_local_symbol_table(elf_file);
        make_symbol_values_from_symbol_table(output_elf_file, local_symbol_table);
    }
}

// Set the executable entrypoint
static void set_entrypoint(RwElfFile *output_elf_file) {
    if (!entrypoint_symbol_name) entrypoint_symbol_name = DEFAULT_ENTRYPOINT_SYMBOL_NAME;
    Symbol *symbol = get_defined_symbol(global_symbol_table, entrypoint_symbol_name);
    if (!symbol) error("Missing %s symbol", entrypoint_symbol_name);
    output_elf_file->entrypoint = symbol->dst_value;
}

// Find the virtual address and size of the TLS template, if present.
static void prepare_tls_template(RwElfFile *output_elf_file) {
    RwSection *tdata_section = get_rw_section(output_elf_file, ".tdata");
    RwSection *tbss_section = get_rw_section(output_elf_file, ".tbss");

    // If there are both .tdata and .bss sections, ensure they are are consecutive
    if (tdata_section && tbss_section && tdata_section->index != tbss_section->index - 1)
        panic(".tdata and .tss sections aren't consecutive: %d != %d - 1", tdata_section->index, tbss_section->index);

    if (tdata_section && !tbss_section) {
        output_elf_file->tls_template_size = tdata_section->size;
        output_elf_file->tls_template_address = tdata_section->address;
    }
    else if (!tdata_section && tbss_section) {
        output_elf_file->tls_template_size = tbss_section->size;
        output_elf_file->tls_template_address = tbss_section->address;
    }
    else if (tdata_section && tbss_section) {
        output_elf_file->tls_template_size = tbss_section->offset - tdata_section->offset + tbss_section->size;
        output_elf_file->tls_template_address = tdata_section->address;
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

            // Only include sections that have program data
            if (!input_section->dst_section) continue;

            const char *section_name = input_section->name;
            RwSection *rw_section = input_section->dst_section;
            if (!rw_section) continue; // The section is not included

            // Allocate memory if not already done in a previous loop
            if (!rw_section->data) rw_section->data = calloc(1, rw_section->size);

            // Load the section data. It may already have been loaded and modified by relocations.
            load_section(input_elf_file, input_section);
            memcpy(rw_section->data + input_section->dst_offset, input_section->data, input_section->size);
        }
    }
}

// Copy the memory for all program sections in the input files to the output file
static void copy_extra_sections_to_output(RwElfFile *output_elf_file) {
    strmap_ordered_foreach(output_elf_file->extra_sections, it) {
        const char *name = strmap_ordered_iterator_key(&it);
        Section *input_section = strmap_ordered_get(output_elf_file->extra_sections, name);

        const char *section_name = input_section->name;
        RwSection *rw_section = input_section->dst_section;
        if (!rw_section) panic("Extra section %s did not get included in the output", section_name);

        // Allocate memory if not already done
        if (!rw_section->data) rw_section->data = calloc(1, rw_section->size);

        // Copy the data
        memcpy(rw_section->data + input_section->dst_offset, input_section->data, input_section->size);
    }
}

RwElfFile *run(List *library_paths, List *linker_scripts, List *input_files, const char *output_filename) {
    // Create output file
    RwElfFile *output_elf_file = init_output_elf_file(output_filename);

    // Setup symbol tables
    init_symbols();

    parse_linker_scripts(output_elf_file, library_paths, linker_scripts);

    // Read input file
    List *input_elf_files = read_input_files(library_paths, input_files);

    // Add some sections that are always present in output ELF file, e.g. .symtab.
    create_default_sections(output_elf_file);

    // Go through the linker script and set the entrypoint symbol
    set_entrypoint_symbol(output_elf_file, input_elf_files);

    // Run through the first pass of the linker script
    layout_input_sections(output_elf_file, input_elf_files);

    // Run through linker script, group sections into program segments, determine section offsets and assign addresses to symbols in the script
    layout_output_sections(output_elf_file, input_elf_files);

    // Make set of all global symbols that have relocations
    make_global_symbols_in_use(output_elf_file, input_elf_files);

    // At this point all symbols should be defined. Ensure this is the case.
    finalize_symbols(output_elf_file);

    // Relax instructions where possible and determine which symbols need to be in the GOT
    apply_relocations(input_elf_files, NULL, RELOCATION_PHASE_SCAN);

    // Create the .got section, if needed
    create_got_section(output_elf_file);

    // Process IFUNCs and create .got.plt, .iplt and .rela.iplt if required
    process_ifuncs(output_elf_file, input_elf_files);

    // Allocate memory for extra sections like .got.plt, .iplt and .rela.iplt
    allocate_extra_sections(output_elf_file);

    // Run through the linker script again to include extra sections
    layout_input_sections(output_elf_file, input_elf_files);

    // If there are any common symbols, create a bss section and allocate values the symbols
    add_common_symbols_to_bss(output_elf_file);

    // Run through linker script again, determine section offsets and assign addresses to symbols in the script
    layout_output_sections(output_elf_file, input_elf_files);

    // Remove sections from the section list that did not get included in the final file
    remove_empty_sections(output_elf_file);

    // Rearrange sections list, so that special sections such as .symtab are moved to the end.
    make_section_indexes(output_elf_file);

    // Add the symbols to the ELF symbol table. The values and section indexes will be updated later.
    make_elf_symbols(output_elf_file);

    // At this point all section sizes are known. Assign offsets to the builtin sections and make the ELF section headers.
    make_elf_section_headers(output_elf_file);

    // Now that the size of the ELF file is known, allocate memory for it.
    allocate_elf_output_memory(output_elf_file);

    // Given a list of sections, group them by type and make the list of program segment headers.
    layout_program_segments(output_elf_file);

    // Make the ELF program segment headers. Include the special TLS section if required.
    make_elf_program_segment_headers(output_elf_file);

    // Assign final values to all symbols
    make_symbol_values(output_elf_file, input_elf_files);

    // Assign values to symbols in the linker script
    make_output_section_command_assignments_symbol_values(output_elf_file);

    // Update the GOT (if there is one)
    update_got_symbol_values(output_elf_file);

    // Make .iplt jmp instructions that refer to the entries in .got.iplt, also finish off .rela.iplt
    update_iplt(output_elf_file);

    // Set the symbol's value and section indexes
    update_elf_symbols(output_elf_file);

    // Set the executable entrypoint
    set_entrypoint(output_elf_file);

    // Check that sections have an increasing offset and don't overlap.
    check_output_sections(output_elf_file);

    // Make the ELF headers and copy the program segment and section headers
    make_elf_headers(output_elf_file);

    // Find the virtual address and size of the TLS template, if present.
    prepare_tls_template(output_elf_file);

    // Copy the memory for all program sections in the input files to the output file
    copy_input_elf_sections_to_output(input_elf_files, output_elf_file);

    // Copy in the extra sections to the output
    copy_extra_sections_to_output(output_elf_file);

    // Write relocated symbol values to the output ELF file
    apply_relocations(input_elf_files, output_elf_file, RELOCATION_PHASE_APPLY);

    // Write the ELF file
    write_elf_file(output_elf_file);

    return output_elf_file;
}
