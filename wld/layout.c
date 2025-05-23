#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "list.h"
#include "ro-elf.h"
#include "rw-elf.h"

#include "wld/layout.h"
#include "wld/script.h"
#include "wld/utils.h"
#include "wld/wld.h"

static SectionsCommandOutput *discard_sections_command_output;

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
    // Also ignore SHF_GROUP aka COMDAT groups.
    int ignore_flags_mask = ~(SHF_MERGE | SHF_STRINGS | SHF_GROUP);
    new_flags &= ignore_flags_mask;

    if (output_section->type != SHT_NULL) {
        // Check for mismatch in type
        if (output_section->type != elf_section_header->sh_type)
            error("Type mismatch in output section %s, file %s, section %s: %d v %d",
                output_section->name, elf_file->filename, file_input_section->name, output_section->type, elf_section_header->sh_type);

        // Check for mismatch in flags
        if ((existing_flags & ignore_flags_mask) != new_flags)
            error("Flags mismatch in output section %s, file %s, section %s: %d v %d",
                output_section->name, elf_file->filename, file_input_section->name, existing_flags, new_flags);
    }

    // Align the offset according to the input section alignment
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
void remove_empty_sections(RwElfFile *output_elf_file) {
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

// Add an assignment that happens in the middle of an output section. The offset relative to the section is
// noted. A loop later on assigns the final address, once all addresses are known.
static void add_output_section_assignment(RwSection *output_section, CommandAssignment *assignment) {
    Symbol *symbol = get_or_add_linker_script_symbol(strdup(assignment->symbol));
    OutputSectionAssignment *output_section_assignment = calloc(1, sizeof(OutputSectionAssignment));
    output_section_assignment->assignment = assignment;
    output_section_assignment->offset = output_section->size;
    if (!strcmp(assignment->symbol, ".")) error_in_file("Assigning to . in a section input isn't implemented");
    append_to_list(output_section->command_assignments, output_section_assignment);
}

// Process output sections commands in a SECTIONS command
// This determines the placement and mapping between input sections and output sections.
// It also creates the output sections.
static void layout_sections_with_script(RwElfFile *output_elf_file, List *input_elf_files, List *section_commands) {
    // Loop over all section commands of typeoutput
    for (int i = 0; i < section_commands->length; i++) {
        SectionsCommand *sections_command = section_commands->elements[i];
        if (sections_command->type == SECTIONS_CMD_ASSIGNMENT) {
            // Create the symbol, but do nothing with it
            CommandAssignment *assignment = &sections_command->assignment;
            get_or_add_linker_script_symbol(strdup(assignment->symbol));
            continue;
        }

        // Implicit else, sections_command->type == SECTIONS_CMD_OUTPUT

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
        output_section->command_assignments = new_list(8);

        // Loop over all script input sections
        List *script_output_items = sections_command->output.output_items;
        for (int j = 0; j < script_output_items->length; j++) {
            SectionsCommandOutputItem *output_item = script_output_items->elements[j];

            if (output_item->type == SECTIONS_CMD_INPUT_ASSIGNMENT) {
                add_output_section_assignment(output_section, &output_item->assignment);
                continue;
            }

            // Implicit else, it's an SECTIONS_CMD_INPUT_SECTION

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

// Process section commands in a linker script, creating output sections and making the mapping
// between input and output sections.
void layout_input_sections(RwElfFile *output_elf_file, List *input_elf_files) {
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
void layout_leftover_sections(RwElfFile *output_elf_file, List *input_elf_files) {
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

// Assign offsets to the builtin sections and make the ELF section headers.
void make_elf_section_headers(RwElfFile *output_elf_file) {
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
        }
    }

    // Check all section offsets are sane
    for (int i = 1; i < output_elf_file->sections_list->length; i++) {
        RwSection *section = output_elf_file->sections_list->elements[i];
        if (!section->offset) panic("Unexpected section offset zero for %s", section->name);
    }

    if (DEBUG_LAYOUT) dump_sections(output_elf_file);

    output_elf_file->size = offset;

    if (DEBUG_LAYOUT)
        printf("ELF file size: %#x\n", output_elf_file->size);
}

// Make an ELF program segment header
static void make_program_segment_header(RwElfFile *output_elf_file, ElfProgramSegmentHeader *psh, RwSection *section) {
    psh->p_flags = PF_R;

    if (section->flags & SHF_WRITE) psh->p_flags |= PF_W;
    if (section->flags & SHF_EXECINSTR) psh->p_flags |= PF_X;

    psh->p_type = PT_LOAD;          // Segment type
}

// Given an input section and an output section, align the input section, update the sizes and process TLS.
// If the current output program segment is suitable to include the input section, update it,
// otherwise, create a new one.
static void layout_one_section_in_executable(RwElfFile *output_elf_file, RwSection *section,  Symbol *dot_symbol, uint64_t *poffset) {
    if (!section) panic("Got NULL output section in layout_one_section_in_executable()");

    if (!section->keep && section->size == 0) return;

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

    // Page align all section offsets to ensure all segment offsets are also page-aligned.
    // This is required to satisfy the ELF alignment rule:
    // p_vaddr % p_align == p_offset % p_align
    *poffset += (dot_symbol->dst_value - *poffset + 0x1000) % 0x1000;

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
    }

    // Save the total size of the .tdata + .tbss sections
    if (section->type == SHT_NOBITS && (section->flags & SHF_TLS)) {
        output_elf_file->tls_template_size = dot_symbol->dst_value - output_elf_file->tls_template_offset + section->size;
        output_elf_file->tls_template_tbss_size = section->size;
    }

    section->layout_complete = 1;
}

// Process an assignment command in a section.
static uint64_t process_assignment(RwElfFile *output_elf_file, Symbol *dot_symbol, CommandAssignment *assignment, uint64_t offset) {
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

    uint64_t number_value = value.symbol ? value.symbol->dst_value : value.number;
    symbol->dst_value = number_value;

    return offset;
}

// Run through linker script, group sections into program segments, determine section offsets and assign addresses to symbols in the script.
// This function is run twice. The first time to collect the symbols in assignments. The offsets and addresses may be incorrect.
// The second time when the final layout is done.
uint64_t layout_output_sections(RwElfFile *output_elf_file) {
    if (DEBUG_LAYOUT) printf("------------------------------------\nLaying out executable\n");

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
                if (!section) panic("Got NULL output section in layout_output_sections()");

                if (!section->keep && section->size == 0) continue;

                layout_one_section_in_executable(output_elf_file, section, dot_symbol, &offset);
            }
        }
    }

    return offset;
}

// Similar to layout_output_sections, do the layout of sections not in the linker script.
void layout_leftover_output_sections(RwElfFile *output_elf_file, List *input_elf_files, uint64_t offset) {
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

            layout_one_section_in_executable(output_elf_file, output_section, dot_symbol, &offset);
        }
    }
}

// Assign values to symbols in the linker script
void make_output_section_command_assignments_symbol_values(RwElfFile *output_elf_file) {
    for (int i = 0; i < output_elf_file->sections_list->length; i++) {
        RwSection *section = output_elf_file->sections_list->elements[i];
        List *command_assignments = section->command_assignments;
        if (!section->command_assignments) continue;
        for (int j = 0; j < command_assignments->length; j++) {
            OutputSectionAssignment *command_assignment = (OutputSectionAssignment *) command_assignments->elements[j];
            Symbol *symbol = must_get_global_defined_symbol(command_assignment->assignment->symbol);
            uint64_t address = section->address + command_assignment->offset;
            symbol->dst_value = address;
        }
    }
}

// Given a list of sections, group them by type and make the list of program segment headers.
void layout_program_segments(RwElfFile *output_elf_file) {
    output_elf_file->program_segments_list = new_list(16);

    ElfProgramSegmentHeader *current_segment = NULL;
    int current_segment_type = -1;
    int current_segment_flags = -1;
    uint64_t previous_section_offset = 0;

    // Loop over all headers
    for (int i = 0; i < output_elf_file->sections_list->length; i++) {
        RwSection *section = output_elf_file->sections_list->elements[i];
        if (section->type == SHT_NULL) continue;
        if (!(section->flags & SHF_ALLOC)) continue;

        // Setup the segment
        if (!current_segment || section->flags != current_segment_flags) {
            // Either the segment is new, or the flags mismatch.
            // Create a new segment.

            if (current_segment && current_segment->p_offset + current_segment->p_filesz > section->offset)
                panic("Overlap in segments: %#lx > %#x", current_segment->p_offset + current_segment->p_filesz, section->offset);

            current_segment = calloc(1, sizeof(ElfProgramSegmentHeader));
            make_program_segment_header(output_elf_file, current_segment, section);
            append_to_list(output_elf_file->program_segments_list, current_segment);

            if (DEBUG_LAYOUT)
                printf("\nCreating new segment using type %#04x and flags %#04x at   %#x  offset %#x\n",
                    section->type, section->flags, section->address, section->offset);

            // Set the program segment offset and address
            current_segment->p_offset = section->offset;
            current_segment->p_vaddr = section->address;
            current_segment->p_paddr = section->address;
            current_segment->p_align = 0x1000;

            current_segment_type = section->type;
            current_segment_flags = section->flags;
            previous_section_offset = section->offset;
        }

        if (DEBUG_LAYOUT)
            printf("Adding %-20s of size %#08x at          %#x  offset %#x\n",
                section->name, section->size, section->address, section->offset);

        // The increase in segment size consists of the difference in alignment + the section size
        uint64_t segment_size_diff = section->offset - previous_section_offset + section->size;

        // Increase the file size unless it's a BSS section
        if (section->type != SHT_NOBITS)
            current_segment->p_filesz += segment_size_diff;

        // Increase the memory size
        current_segment->p_memsz += segment_size_diff;

        previous_section_offset = section->offset + section->size;
    }
}