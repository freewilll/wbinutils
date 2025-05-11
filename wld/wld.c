#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "list.h"
#include "ro-elf.h"
#include "rw-elf.h"

#include "wld/libs.h"
#include "wld/relocations.h"
#include "wld/script.h"
#include "wld/symbols.h"
#include "wld/utils.h"
#include "wld/wld.h"

static char *entrypoint_symbol_name;
static SectionsCommandOutput *discard_sections_command_output;

static const char *PROGRAM_SEGMENT_TYPE_NAMES[] = { "PT_NULL", "PT_LOAD", "PT_DYNAMIC", "PT_INTERP", "PT_NOTE", "PT_SHLIB", "PT_PHDR", "PT_TLS", "PT_NUM" };

// Go down all input files which are either object files or libraries
static List *read_input_files(List *library_paths, List *input_files) {
    List *input_elf_files = new_list(32);

    for (int i = 0; i < input_files->length; i++) {
        InputFile *input_file = input_files->elements[i];
        char *input_filename = input_file->filename;
        if (DEBUG_SYMBOL_RESOLUTION) printf("Examining file %s\n", input_filename);

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

// Given an input section, add it to an output section. The output section may be empty, in that case itis of type SHT_NULL.
// If not empty, it checks if the input section is compatible.
// If all is well, the offset, size and alignment is determined.
static void add_input_to_output_section(RwSection *output_section, ElfFile *elf_file, Section *file_input_section) {
    ElfSectionHeader *elf_section_header = file_input_section->elf_section_header;

    // Associate the input and output sections
    file_input_section->dst_section = output_section;

    // Check type and flags
    int existing_flags = output_section->flags;
    int new_flags = elf_section_header->sh_flags;

    // Discard merge and strings flags, they are unimportant when it comes to merging sections.
    new_flags &= ~(SHF_MERGE | SHF_STRINGS);

    if (output_section->type != SHT_NULL) {
        // Check for mismatch in type
        if (output_section->type != elf_section_header->sh_type)
            error("Type mismatch in output section %s, file %s, section %s: %d v %d",
                output_section->name, elf_file->filename, file_input_section->name, output_section->type, elf_section_header->sh_type);

        // Check for mismatch in flags
        if (existing_flags != new_flags)
            error("Flags mismatch in output section %s, file %s, section %s: %d v %d",
                output_section->name, elf_file->filename, file_input_section->name, existing_flags, new_flags);
    }

    // Align the offset accotding to the input section alignment
    int offset = ALIGN_UP(output_section->size, elf_section_header->sh_addralign);
    file_input_section->offset = offset;

    // Configure the output section
    output_section->size = offset + elf_section_header->sh_size;
    output_section->type = elf_section_header->sh_type;
    output_section->flags = new_flags;
    output_section->align = MAX(output_section->align, elf_section_header->sh_addralign);

    if (DEBUG_LAYOUT || DEBUG_RELOCATIONS)
        printf("  File %-50s section %-20s size %#08lx is at offset %#08x in target section %s\n",
            elf_file->filename, file_input_section->name, elf_section_header->sh_size, offset, output_section->name);
}

// Remove sections from the section list that did not get included in the final file.
// Due to implementation details in the layout functions, empty sections can make it in the sections list.
// Discard them, unless they have the keep flag set.
static void remove_empty_sections(RwElfFile *output_elf_file) {
    List *new_section_list = new_list(output_elf_file->sections_list->length);

    for (int i = 0; i < output_elf_file->sections_list->length; i++) {
        RwSection *section = output_elf_file->sections_list->elements[i];

        // Only include the first NULL section. The other NULL sections are artifacts of the layout and
        // are discarded here.
        if (i == 0 || section->type == SHT_STRTAB || section->type == SHT_SYMTAB || section->keep || section->size) {
            append_to_list(new_section_list, section);
        }
        else {
            strmap_delete(output_elf_file->sections_map, section->name);
        }
    }

    output_elf_file->sections_list = new_section_list;
}

// Process output sections commands in a SECTIONS command
// This determines the placement and mapping between input sections and output sections.
// It also creates the output sections.
static void layout_sections_with_script(RwElfFile *output_elf_file, List *input_elf_files, List *section_commands) {
    // Loop over all section commands of typeoutput
    for (int i = 0; i < section_commands->length; i++) {
        SectionsCommand *sections_command = section_commands->elements[i];
        if (sections_command->type != SECTIONS_CMD_OUTPUT) continue;

        char *output_section_name = sections_command->output.output_section_name;

        if (DEBUG_LAYOUT)
            printf("Output section %s\n", output_section_name);

        // If a /DISCARD/ is present, save it, and don't treat it like an actual section.
        if (!strcmp(output_section_name, LINKER_SCRIPT_DISCARD_OUTPUT_SECTION_NAME)) {
            discard_sections_command_output = &sections_command->output;
            continue;
        }

        // Create the initial output section
        RwSection *output_section = add_rw_section(output_elf_file, output_section_name, SHT_NULL, 0, 0);

        // Loop over all script input sections
        List *script_output_items = sections_command->output.output_items;
        for (int j = 0; j < script_output_items->length; j++) {
            SectionsCommandOutputItem *output_item = script_output_items->elements[j];

            if (output_item->type != SECTIONS_CMD_INPUT_SECTION) continue;

            InputSection *script_input_section = &output_item->input_section;

            // Loop over all input files
            for (int k = 0; k < input_elf_files->length; k++) {
                ElfFile *elf_file = input_elf_files->elements[k];

                // Check the input file matches the pattern
                if (!match_pattern(elf_file->filename, script_input_section->file_pattern)) continue;

                // Match script input section with file input section
                List *section_patterns = script_input_section->section_patterns;
                for (int k = 0; k < section_patterns->length; k++) {
                    char *section_pattern = section_patterns->elements[k];

                    if (!strcmp(section_pattern, LINKER_SCRIPT_COMMON_SECTION_NAME)) {
                        // Process *(COMMON) input section.
                        // Make note of bss section that will get the common symbols in it.
                        output_elf_file->section_bss = output_section;
                        continue;
                    }

                    // Loop over all sections in the input file
                    for (int l = 0; l < elf_file->section_list->length; l++) {
                        Section *file_input_section = (Section *) elf_file->section_list->elements[l];
                        ElfSectionHeader *elf_section_header = file_input_section->elf_section_header;

                        // Check the section name matches the pattern
                        if (!match_pattern(file_input_section->name, section_pattern)) continue;

                        // Discard empty sections not to be kept straight away
                        if (!script_input_section->keep && !elf_section_header->sh_size) continue;

                        // Ensure the output section isn't thrown away later on if keep=1
                        if (script_input_section->keep) output_section->keep = 1;

                        if (file_input_section->dst_section) continue; // Already included

                        add_input_to_output_section(output_section, elf_file, file_input_section);
                    }
                }
            }
        }
    }
}

// Go through the linker script and set the entrypoint symbol
static void set_entrypoint_symbol(RwElfFile *output_elf_file, List *input_elf_files) {
    for (int i = 0; i < linker_script->length; i++) {
        ScriptCommand *script_command = linker_script->elements[i];

        if (script_command->type == CMD_ENTRY)
            entrypoint_symbol_name = script_command->entry.symbol;
    }
}

// Process section commands in a linker script, creating output sections and making the mapping
// between input and output sections.
static void layout_input_sections(RwElfFile *output_elf_file, List *input_elf_files) {
    for (int i = 0; i < linker_script->length; i++) {
        ScriptCommand *script_command = linker_script->elements[i];

        if (script_command->type == CMD_SECTIONS) {
            List *section_commands = script_command->sections.commands;
            layout_sections_with_script(output_elf_file, input_elf_files, section_commands);
        }
    }
}

// Does an input filename and section match one of the /DISCARD/ patterns (if present)?
static int is_discarded_section(const char *input_filename, const char *input_section_name) {
    if (!discard_sections_command_output) return 0;

    // Loop over all script input sections
    List *output_items = discard_sections_command_output->output_items;
    for (int i = 0; i < output_items->length; i++) {
        SectionsCommandOutputItem *output_item = output_items->elements[i];
        if (output_item->type != SECTIONS_CMD_INPUT_SECTION) continue;
        InputSection *input_section = &output_item->input_section;

        if (!match_pattern(input_filename, input_section->file_pattern)) continue;

        // Match script input section with file input section
        List *section_patterns = input_section->section_patterns;
        for (int j = 0; j < section_patterns->length; j++) {
            char *section_pattern = section_patterns->elements[j];
            if (!match_pattern(input_section_name, section_pattern)) continue;

            // The discard pattern matches
            return 1;
        }
    }

    return 0;
}

// Loop over all sections in the input files and create the target sections in the output file if not already there
static void layout_leftover_sections(RwElfFile *output_elf_file, List *input_elf_files) {
    // Loop over all input files
    for (int i = 0; i < input_elf_files->length; i++) {
        ElfFile *elf_file = input_elf_files->elements[i];

        // Loop over all sections
        for (int j = 0; j < elf_file->section_list->length; j++) {
            Section *input_section  = (Section *) elf_file->section_list->elements[j];
            if (input_section->dst_section) continue; // Already placed

            ElfSectionHeader *elf_section_header = input_section->elf_section_header;
            const char *section_name = &elf_file->section_header_strings[elf_section_header->sh_name];

            // Only include certain sections
            if (!ORPHANED_SECTION_TYPE(input_section->elf_section_header->sh_type)) continue;

            // Discard sections in the /DISCARD/ section
            if (is_discarded_section(elf_file->filename, section_name)) continue;

            // Create the output section if it doesn't already exist
            RwSection *output_section = get_rw_section(output_elf_file, section_name);
            if (!output_section)
                output_section = add_rw_section(output_elf_file, section_name,
                    elf_section_header->sh_type, elf_section_header->sh_flags, elf_section_header->sh_addralign);

            // Add the section to the output
            add_input_to_output_section(output_section, elf_file, input_section);
        }
    }
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


// Make an ELF program segment header
static void make_program_segment_header(RwElfFile *output_elf_file, ElfProgramSegmentHeader *psh, RwSection *section) {
    psh->p_flags = PF_R;

    if (section->flags & SHF_WRITE) psh->p_flags |= PF_W;
    if (section->flags & SHF_EXECINSTR) psh->p_flags |= PF_X;

    psh->p_type = PT_LOAD;          // Segment type
    psh->p_filesz = section->size;  // Segment size in file
    psh->p_memsz = section->size;   // Segment size in memory
}

// Assign offsets to the builtin sections and make the ELF section headers.
static void make_elf_section_headers(RwElfFile *output_elf_file) {
    // Allocate memory for section headers
    output_elf_file->elf_section_headers_size = sizeof(ElfSectionHeader) * output_elf_file->sections_list->length;
    output_elf_file->elf_section_headers = calloc(1, output_elf_file->elf_section_headers_size);

    // Loop over all headers
    for (int i = 0; i < output_elf_file->sections_list->length; i++) {
        RwSection *section = output_elf_file->sections_list->elements[i];
        make_rw_section_header(output_elf_file, &output_elf_file->elf_section_headers[i], section);
        output_elf_file->elf_section_headers[i].sh_addr = section->address;
    }

    // Assign offsets to last 3 sections
    uint64_t offset = -1;
    for (int i = 0; i < output_elf_file->sections_list->length; i++) {
        RwSection *section = output_elf_file->sections_list->elements[i];
        if (!strcmp(section->name, ".symtab") || !strcmp(section->name, ".strtab") || !strcmp(section->name, ".shstrtab")) {
            if (offset == -1) {
                if (i == 0) error("Trying to link an ELF file with no contents");
                RwSection *previous_section = output_elf_file->sections_list->elements[i - 1];
                offset = previous_section->offset + previous_section->size;
            }
            offset = ALIGN_UP(offset, section->align);
            section->offset = offset;
            output_elf_file->elf_section_headers[i].sh_offset = offset;
            offset += section->size;

            if (DEBUG_LAYOUT)
                printf("Setting offset for %s to %#x size=%x name=%d\n",
                    section->name, section->offset, section->size, output_elf_file->elf_section_headers[i].sh_name);
        }
    }

    // Check all section offsets are sane
    for (int i = 1; i < output_elf_file->sections_list->length; i++) {
        RwSection *section = output_elf_file->sections_list->elements[i];
        if (!section->offset) panic("Unexpected section offset zero for %s", section->name);
    }

    if (DEBUG_LAYOUT) {
        printf("Section headers:\n");

        for (int i = 0; i < output_elf_file->sections_list->length; i++) {
            RwSection *section = output_elf_file->sections_list->elements[i];
            printf("%3d %-40s type %02x  flags %02x  offset %#08x   address %#08x   size %#08x\n", i, section->name, section->type, section->flags, section->offset, section->address, section->size);
        }
    }

    output_elf_file->size = offset;

    if (DEBUG_LAYOUT)
        printf("ELF file size: %#x\n", output_elf_file->size);
}

void allocate_elf_output_memory(RwElfFile *output_elf_file) {
    output_elf_file->data = calloc(1, output_elf_file->size);
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
        tls_program_segment->p_align = 0x1000;

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


    if (DEBUG_LAYOUT)
        printf("\nSegments:\n");

    for (int i = 0; i < output_elf_file->program_segments_list->length; i++) {
        ElfProgramSegmentHeader *program_segment = output_elf_file->program_segments_list->elements[i];

        memcpy(&output_elf_file->elf_program_segment_headers[i + 1], program_segment, sizeof(ElfProgramSegmentHeader));

        if (DEBUG_LAYOUT)
            printf("%3d %-11s  flags %02x  offset %#08lx   address %#08lx  filesz %#08lx  memsz %#08lx align %#08lx\n",
                i, PROGRAM_SEGMENT_TYPE_NAMES[program_segment->p_type], program_segment->p_flags, program_segment->p_offset, program_segment->p_vaddr,
                program_segment->p_filesz, program_segment->p_memsz, program_segment->p_align);
    }

    // Copy program headers
    memcpy(
        output_elf_file->data + output_elf_file->elf_program_segments_offset,
        output_elf_file->elf_program_segment_headers,
        output_elf_file->elf_program_segments_header_size);
}

// Given an input section and an output section, align the input section, update the sizes and process TLS.
// If the current output program segment is suitable to include the input section, update it,
// otherwise, create a new one.
static void layout_one_section_in_executable(RwElfFile *output_elf_file,
        ElfProgramSegmentHeader **pcurrent_segment, RwSection *section, int *pcurrent_segment_type, int *pcurrent_segment_flags,
        Symbol *dot_symbol, uint64_t *poffset) {

    if (!section) panic("Got NULL output section in layout_one_section_in_executable()");

    if (!section->keep && section->size == 0) return;

    if (!*pcurrent_segment || section->flags != *pcurrent_segment_flags) {
        // Either the segment is new, or the flags mismatch.
        // Create a new segment.

        *pcurrent_segment = calloc(1, sizeof(ElfProgramSegmentHeader));
        make_program_segment_header(output_elf_file, *pcurrent_segment, section);
        append_to_list(output_elf_file->program_segments_list, *pcurrent_segment);

        if (DEBUG_LAYOUT)
            printf("\nCreating new segment using type %#04x and flags %#04x at   %#x  offset %#lx\n",
                section->type, section->flags, dot_symbol->dst_value, *poffset);

        // Set the program segment offset and address
        (*pcurrent_segment)->p_offset = *poffset;
        (*pcurrent_segment)->p_vaddr = dot_symbol->dst_value;
        (*pcurrent_segment)->p_paddr = dot_symbol->dst_value;
        (*pcurrent_segment)->p_align = 0x1000;

        *pcurrent_segment_type = section->type;
        *pcurrent_segment_flags = section->flags;
    }

    // Align the section
    uint64_t old_offset = *poffset;

    // Align TLS .tdata section to page boundaries, but move the data to the end of the section
    if (section->type == SHT_PROGBITS && (section->flags & SHF_TLS)) {
        uint64_t offset_end = ALIGN_UP(*poffset + section->size, 0x1000);
        *poffset = ALIGN_DOWN(offset_end - section->size, section->align);

        uint64_t address_end = ALIGN_UP(dot_symbol->dst_value + section->size, 0x1000);
        dot_symbol->dst_value = ALIGN_DOWN(address_end - section->size, section->align);

        output_elf_file->tls_template_address = dot_symbol->dst_value;
        output_elf_file->tls_template_offset = *poffset;
        output_elf_file->tls_template_tdata_size = section->size;
    }

    else if (section->align != 0) {
        dot_symbol->dst_value = ALIGN_UP(dot_symbol->dst_value, section->align);

        if (section->type != SHT_NOBITS)
            *poffset = ALIGN_UP(*poffset, section->align);
    }

    if (DEBUG_LAYOUT)
        printf("Adding %-20s of size %#08x at          %#x  offset %#lx\n",
            section->name, section->size, dot_symbol->dst_value, *poffset);

    // Set the section offset and address
    section->offset = *poffset;
    section->address = dot_symbol->dst_value;

    // Advance the . symbol
    dot_symbol->dst_value += section->size;

    // The increase in segment size consists of the difference in alignment + the section size
    uint64_t segment_size_diff = *poffset - old_offset + section->size;

    // Increase the file size unless it's a BSS section
    if (section->type != SHT_NOBITS) {
        *poffset += section->size;
        (*pcurrent_segment)->p_filesz += segment_size_diff;
    }

    // Increase the memory size
    (*pcurrent_segment)->p_memsz += segment_size_diff;

    // Save the total size of the .tdata + .tbss sections
    if (section->type == SHT_NOBITS && (section->flags & SHF_TLS)) {
        output_elf_file->tls_template_size = dot_symbol->dst_value - output_elf_file->tls_template_offset + section->size;
        output_elf_file->tls_template_tbss_size = section->size;
    }

    section->layout_complete = 1;
}

// Process an assignment command in a section.
uint64_t process_assignment(RwElfFile *output_elf_file, Symbol *dot_symbol, CommandAssignment *assignment, uint64_t offset) {
    Value value = evaluate_node(assignment->node, output_elf_file);
    Symbol *symbol = get_or_add_linker_script_symbol(strdup(assignment->symbol));

    // Special case for the dot symbol. On first assignment, align the offset to it.
    // On second assignment, move the offset along by the same amount.
    // The linker script is constrained such that . must always increase.
    if (symbol == dot_symbol) {
        if (dot_symbol->dst_value) {
            // Move offset along by the same amount that the address increased.
            int64_t diff = value.number - dot_symbol->dst_value;
            if (diff < 0) panic("Linker script address went backwards: %d", diff);
            offset += diff;
        }
        else {
            // Align initial offset to address
            offset = value.number & 0xfff;
        }
    }

    symbol->dst_value = value.number;

    return offset;
}

// Run through linker script, group sections into program segments, determine section offsets and assign addresses to symbols in the script.
// This function is run twice. The first time to collect the symbols in assignments. The offsets and addresses may be incorrect.
// The second time when the final layout is done.
static uint64_t layout_output_segments_and_sections(RwElfFile *output_elf_file) {
    if (DEBUG_LAYOUT) printf("------------------------------------\nLaying out executable\n");

    output_elf_file->program_segments_list = new_list(16);

    ElfProgramSegmentHeader *current_segment = NULL;
    int current_segment_type = -1;
    int current_segment_flags = -1;
    uint64_t offset = 0;

    Symbol *dot_symbol = get_or_add_linker_script_symbol(strdup("."));
    dot_symbol->dst_value = 0; // Zero indicates no offset has been set yet.

    for (int i = 0; i < linker_script->length; i++) {
        ScriptCommand *script_command = linker_script->elements[i];

        if (script_command->type != CMD_SECTIONS) continue;
        List *section_commands = script_command->sections.commands;

        for (int j = 0; j < section_commands->length; j++) {
            SectionsCommand *sections_command = section_commands->elements[j];
            if (sections_command->type == SECTIONS_CMD_ASSIGNMENT) {
                CommandAssignment *assignment = &sections_command->assignment;
                offset = process_assignment(output_elf_file, dot_symbol, assignment, offset);
            }

            else if (sections_command->type == SECTIONS_CMD_OUTPUT) {
                char *section_name = sections_command->output.output_section_name;

                if (!strcmp(section_name, LINKER_SCRIPT_DISCARD_OUTPUT_SECTION_NAME)) continue;
                RwSection *section = get_rw_section(output_elf_file, section_name);
                if (!section) panic("Got NULL output section in layout_output_segments_and_sections()");

                if (!section->keep && section->size == 0) continue;

                layout_one_section_in_executable(output_elf_file,
                    &current_segment, section, &current_segment_type, &current_segment_flags,
                    dot_symbol, &offset);
            }
        }
    }

    return offset;
}

// Similar to layout_output_segments_and_sections, do the layout of sections not in the linker script.
static void layout_leftover_output_segments_and_sections(RwElfFile *output_elf_file, List *input_elf_files, uint64_t offset) {
    ElfProgramSegmentHeader *current_segment = NULL;
    int current_segment_type = -1;
    int current_segment_flags = -1;

    Symbol *dot_symbol = get_or_add_linker_script_symbol(strdup("."));

    // Loop over all files
    for (int i = 0; i < input_elf_files->length; i++) {
        ElfFile *elf_file = input_elf_files->elements[i];

        // Loop over all sections
        for (int j = 0; j < elf_file->section_list->length; j++) {
            Section *input_section  = (Section *) elf_file->section_list->elements[j];
            RwSection *output_section = input_section->dst_section;
            if (!output_section) continue; // Not included
            if (output_section->layout_complete) continue;

            if (!output_section->keep && output_section->size == 0) continue;

            layout_one_section_in_executable(output_elf_file,
                &current_segment, output_section, &current_segment_type, &current_segment_flags,
                dot_symbol, &offset);
        }
    }
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

// If there are any common symbols, create a bss section and allocate values the symbols
static void add_common_symbols_to_bss(RwElfFile *output) {
    if (!common_symbols_are_present()) return;

    // Create the .bss section
    if (!output->section_bss)
        output->section_bss = add_rw_section(output, ".bss", SHT_NOBITS, SHF_ALLOC | SHF_WRITE, 0);

    layout_common_symbols_in_bss_section(output->section_bss);
}

// Assign final values to all symbols
static void make_symbol_values(List *input_elf_files, RwElfFile *output_elf_file) {
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

static void make_array_symbol_values(RwElfFile *output_elf_file) {
    for (int i = 0; i < output_elf_file->sections_list->length; i++) {
        RwSection *section = output_elf_file->sections_list->elements[i];

        #define SET_START_END(start_symbol_name, end_symbol_name) { \
            Symbol *start_symbol = must_get_global_defined_symbol(start_symbol_name); \
            Symbol *end_symbol = must_get_global_defined_symbol(end_symbol_name); \
            start_symbol->dst_value = section->address; \
            end_symbol->dst_value = section->address + section->size; \
            start_symbol->dst_section = section; \
            end_symbol->dst_section = section; \
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
            ElfSectionHeader *input_elf_section_header = input_section->elf_section_header;

            // Only include sections that have program data
            if (!input_section->dst_section) continue;

            const char *section_name = &input_elf_file->section_header_strings[input_elf_section_header->sh_name];
            RwSection *rw_section = input_section->dst_section;
            if (!rw_section) continue; // The section is not included

            // Allocate memory if not already done in a previous loop
            if (!rw_section->data) rw_section->data = calloc(1, rw_section->size);

            // Load the section data. It may already have been loaded and modified by relocations.
            load_section(input_elf_file, input_section);
            memcpy(rw_section->data + input_section->offset, input_section->data, input_section->elf_section_header->sh_size);
        }
    }
}

void run(List *library_paths, List *linker_scripts, List *input_files, const char *output_filename) {
    // Create output file
    RwElfFile *output_elf_file = new_rw_elf_file(output_filename, ET_EXEC);

    // Setup symbol tables
    init_symbols();

    parse_linker_scripts(library_paths, linker_scripts);

    // Read input file
    List *input_elf_files = read_input_files(library_paths, input_files);

    // Add some sections that are always present in output ELF file, e.g. .symtab.
    create_default_sections(output_elf_file);

    // Go through the linker script and set the entrypoint symbol
    set_entrypoint_symbol(output_elf_file, input_elf_files);

    // Run through the first pass of the linker script
    layout_input_sections(output_elf_file, input_elf_files);

    // Add any sections not present in the linker script
    layout_leftover_sections(output_elf_file, input_elf_files);

    // Run through linker script, group sections into program segments, determine section offsets and assign addresses to symbols in the script
    layout_output_segments_and_sections(output_elf_file);

    // At this point all symbols should be defined. Ensure this is the case.
    finalize_symbols();

    // Relax instructions where possible and determine which symbols need to be in the GOT
    apply_relocations(input_elf_files, NULL, RELOCATION_PHASE_SCAN);

    // Create the .got section, if needed
    create_global_offset_table(output_elf_file);

    // If there are any common symbols, create a bss section and allocate values the symbols
    add_common_symbols_to_bss(output_elf_file);

    // Run through linker script again, determine section offsets and assign addresses to symbols in the script
    uint64_t offset = layout_output_segments_and_sections(output_elf_file);

    // Similar to layout_output_segments_and_sections, do the layout of sections not in the linker script.
    layout_leftover_output_segments_and_sections(output_elf_file, input_elf_files, offset);

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

    // Make the ELF program segment headers. Include the special TLS section if required.
    make_elf_program_segment_headers(output_elf_file);

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

    // Make the ELF headers and copy the program segment and section headers
    make_elf_headers(output_elf_file);

    // Find the virtual address and size of the TLS template, if present.
    prepare_tls_template(output_elf_file);

    // Copy the memory for all program sections in the input files to the output file
    copy_input_elf_sections_to_output(input_elf_files, output_elf_file);

    // Write relocated symbol values to the output ELF file
    apply_relocations(input_elf_files, output_elf_file, RELOCATION_PHASE_APPLY);

    // Write the ELF file
    write_elf_file(output_elf_file);
}
