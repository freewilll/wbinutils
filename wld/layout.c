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

#define IGNORE_FLAGS_MASK (~(SHF_MERGE | SHF_STRINGS | SHF_GROUP))

static int input_and_output_sections_match(RwSection *output_section, Section *input_section, int fail_on_mismatch, ElfFile *maybe_elf_file) {
    int existing_flags = output_section->flags;
    int new_flags = input_section->flags;

    // Check for mismatch in type
    if (output_section->type != input_section->type) {
        if (fail_on_mismatch)
            error("Type mismatch in output section %s, file %s, section %s: %d v %d",
                output_section->name,
                maybe_elf_file ? maybe_elf_file->filename : "-",
                input_section->name, output_section->type, input_section->type);
        else
            return 0;
    }

    // Check for mismatch in flags
    if ((existing_flags & IGNORE_FLAGS_MASK) != (new_flags & IGNORE_FLAGS_MASK)) {
        if (fail_on_mismatch)
            error("Flags mismatch in output section %s, file %s, section %s: %d v %d",
                output_section->name,
                maybe_elf_file ? maybe_elf_file->filename : "-",
                input_section->name, existing_flags, new_flags);
        else
            return 0;
    }

    return 1;
}

// Given an input section, add it to an output section. The output section may be empty, in that case itis of type SHT_NULL.
// If not empty, it checks if the input section is compatible.
// If all is well, the offset, size and alignment is determined.
// elf_file may be null
static void add_input_to_output_section(RwSection *output_section, Section *input_section, ElfFile *maybe_elf_file) {
    // Associate the input and output sections
    input_section->dst_section = output_section;

    // Check type and flags
    int existing_flags = output_section->flags;
    int new_flags = input_section->flags;

    // Discard merge and strings flags, they are unimportant when it comes to merging sections.
    // Also ignore SHF_GROUP aka COMDAT groups.
    new_flags &= IGNORE_FLAGS_MASK;

    if (output_section->type != SHT_NULL)
        input_and_output_sections_match(output_section, input_section, 1, maybe_elf_file);

    // Align the offset according to the input section alignment
    int offset = ALIGN_UP(output_section->size, input_section->align);
    input_section->dst_offset = offset;

    // Configure the output section
    output_section->size = offset + input_section->size;
    output_section->type = input_section->type;
    output_section->flags = new_flags;
    output_section->align = MAX(output_section->align, input_section->align);

    if ((DEBUG_LAYOUT || DEBUG_RELOCATIONS) && input_section->size)
        printf("  File %-50s section %-20s size %#08lx is at offset %#08x new size=%#08x in target section %s\n",
            maybe_elf_file ? maybe_elf_file->filename :  "-",
            input_section->name, input_section->size, offset, output_section->size, output_section->name);
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

// Set discard_sections_command_output variable to the contents of a /DISCARD/ sectiom, if present in the script.
static void make_discard_sections_command_output(List *section_commands) {
    // Loop over all section commands of type output
    for (int i = 0; i < section_commands->length; i++) {
        SectionsCommand *sections_command = section_commands->elements[i];
        if (sections_command->type != SECTIONS_CMD_INPUT_SECTION) continue;

        char *output_section_name = sections_command->output.output_section_name;

        // If a /DISCARD/ is present, save it, and don't treat it like an actual section.
        if (!strcmp(output_section_name, LINKER_SCRIPT_DISCARD_OUTPUT_SECTION_NAME)) {
            discard_sections_command_output = &sections_command->output;
            return;
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

        if (!match_path_pattern(input_filename, input_section->file_pattern)) continue;

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
// This function can be run multiple times. Running it twice in a row should produce the same results.
static void layout_sections_with_script(RwElfFile *output_elf_file, List *input_elf_files, List *section_commands) {
    // Loop over all section commands of type output
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

        if (!strcmp(output_section_name, LINKER_SCRIPT_DISCARD_OUTPUT_SECTION_NAME)) continue;

        if (DEBUG_LAYOUT)
            printf("Output section %s\n", output_section_name);

        // A section might already exist
        RwSection *output_section = get_rw_section(output_elf_file, output_section_name);
        if (!output_section)
            // Create the initial output section
            output_section = add_rw_section(output_elf_file, output_section_name, SHT_NULL, 0, 0);

        // Reset the output section from a potential previous run
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
            List *section_patterns = script_input_section->section_patterns;

            // Match script input section with file input section
            for (int l = 0; l < section_patterns->length; l++) {
                char *section_pattern = section_patterns->elements[l];

                if (!strcmp(section_pattern, LINKER_SCRIPT_COMMON_SECTION_NAME)) {
                    // Process *(COMMON) input section.
                    // Make note of bss section that will get the common symbols in it.
                    output_elf_file->section_bss = output_section;
                    continue;
                }

                // Loop over all input files
                for (int k = 0; k < input_elf_files->length; k++) {
                    ElfFile *elf_file = input_elf_files->elements[k];

                    // Check the input file matches the pattern
                    if (!match_path_pattern(elf_file->filename, script_input_section->file_pattern)) continue;

                    // Loop over all sections in the input file
                    for (int l = 0; l < elf_file->section_list->length; l++) {
                        Section *file_input_section = (Section *) elf_file->section_list->elements[l];

                        if (file_input_section->dst_section) continue; // Already included

                        // Check the section name matches the pattern
                        if (!match_pattern(file_input_section->name, section_pattern)) continue;

                        // Ensure the output section isn't thrown away later on if keep=1
                        if (script_input_section->keep) output_section->keep = 1;

                        add_input_to_output_section(output_section, file_input_section, elf_file);
                    }
                }

                // See if anything in extra_sections matches the pattern
                strmap_ordered_foreach(output_elf_file->extra_sections, it) {
                    const char *name = strmap_ordered_iterator_key(&it);
                    Section *section = strmap_ordered_get(output_elf_file->extra_sections, name);
                    if (!match_pattern(section->name, section_pattern)) continue;
                    add_input_to_output_section(output_section, section, NULL);
                }
            } // Output items loop
        } // Input sections loop
    } // Loop over section commands
}

// Given an output section at the end of the file. Scan all existing sections and find the first batch of sections
// with compatible type and flags that it can be placed after.
// This is an optimization, that allows for less program segments to be created, since orphan sections
// can be merged with preceding compatible ones.
static void move_output_section_after_similar_ones(RwElfFile *output_elf_file, RwSection *output_section) {
    int position = -1; // Start with no compatible section found

    for (int i = 0; i < output_elf_file->sections_list->length; i++) {
        RwSection *existing_output_section = output_elf_file->sections_list->elements[i];
        // If we get to the last section and nothing compatible was found, then do nothing
        if (existing_output_section == output_section) return;

        int existing_flags = existing_output_section->flags;
        int flags = output_section->flags;
        int existing_type = existing_output_section->type;
        int type = output_section->type;
        int flags_match = existing_type == type && (existing_flags & IGNORE_FLAGS_MASK) == (flags & IGNORE_FLAGS_MASK);

        if (flags_match)
            position = i; // Advance the position to the current section
        else if (position != -1)
            break; // We have a match already, but this section isn't compatible. Stop looping.
    }

    if (position == -1) panic("Unexpectedly didn't find a place for %s", output_section->name);

    move_rw_section(output_elf_file, output_section, position + 1);
}

// Loop over all sections in the input files and create the target sections in the output file if not already there.
// This happens for sections that don't match anything in the linker script and could not
// be merged into an existing sections due to differing flags.
static void layout_orphan_sections(RwElfFile *output_elf_file, List *input_elf_files) {
    // Loop over all input files
    for (int i = 0; i < input_elf_files->length; i++) {
        ElfFile *elf_file = input_elf_files->elements[i];

        // Loop over all sections
        for (int j = 0; j < elf_file->section_list->length; j++) {
            Section *input_section  = (Section *) elf_file->section_list->elements[j];
            if (input_section->dst_section) continue; // Already placed

            const char *section_name = input_section->name;

            // Only include certain sections
            if (!ORPHANED_SECTION_TYPE(input_section->type)) continue;

            // Discard sections in the /DISCARD/ section
            if (is_discarded_section(elf_file->filename, section_name)) continue;

        if (DEBUG_LAYOUT)
            printf("Leftover output section %s\n", section_name);

            // Create the output section if it doesn't already exist
            RwSection *output_section = get_rw_section(output_elf_file, section_name);
            if (!output_section) {
                output_section = add_rw_section(output_elf_file, section_name,
                    input_section->type, input_section->flags, input_section->align);
                move_output_section_after_similar_ones(output_elf_file, output_section);
                output_section->is_orphan = 1;
            }

            // Add the section to the output
            add_input_to_output_section(output_section, input_section, elf_file);
        }
    }
}

// Process section commands in a linker script, creating output sections and making the mapping
// between input and output sections.
void layout_input_sections(RwElfFile *output_elf_file, List *input_elf_files) {
    // Reset section sizes
    for (int i = 0; i < output_elf_file->sections_list->length; i++) {
        RwSection *section = output_elf_file->sections_list->elements[i];

        // If it's not a symbol table or string table, set size to zero
        if (section->type != SHT_SYMTAB && section->type != SHT_STRTAB)
            section->size = 0;
    }

    // Reset dst sections
    for (int i = 0; i < input_elf_files->length; i++) {
        ElfFile *elf_file = input_elf_files->elements[i];

        // Loop over all sections
        for (int j = 0; j < elf_file->section_list->length; j++) {
            Section *input_section  = (Section *) elf_file->section_list->elements[j];
            input_section->dst_section = NULL;
        }
    }

    for (int i = 0; i < linker_script->length; i++) {
        ScriptCommand *script_command = linker_script->elements[i];

        if (script_command->type == CMD_SECTIONS) {
            List *section_commands = script_command->sections.commands;
            make_discard_sections_command_output(section_commands);
            layout_sections_with_script(output_elf_file, input_elf_files, section_commands);
        }
    }

    layout_orphan_sections(output_elf_file, input_elf_files);
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
        if (!section->offset) panic("Unexpected section offset zero for output section %s", section->name);
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
static void layout_one_section_in_executable(RwElfFile *output_elf_file, RwSection *section, RwSection *last_section, Symbol *dot_symbol, uint64_t *poffset) {
    if (!section) panic("Got NULL output section in layout_one_section_in_executable()");

    if (!section->keep && section->size == 0) return;

    // Align the section
    uint64_t old_offset = *poffset;

    // if the section cannot be merged with the last one, then
    // page align the start of a section to ensure kernel memory mappings
    // never overlap. This is a bit hack to do it here, since it's the program segment
    // that needs to be page aligned, not the section.
    int need_page_alignment = 0;

    // NOBITS sections are allowed to be merged after non-NOBITS sections, but not the other way around.
    if (last_section && last_section->type == SHT_NOBITS && section->type != SHT_NOBITS) need_page_alignment = 1;

    // A flags mismatch requires page alignment
    if (last_section && (section->flags & IGNORE_FLAGS_MASK) != (last_section->flags & IGNORE_FLAGS_MASK)) need_page_alignment = 1;

    if (need_page_alignment) {
        dot_symbol->dst_value = ALIGN_UP(dot_symbol->dst_value, 0x1000);
        *poffset = ALIGN_UP(*poffset, 0x1000);
    }

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
    if (section->type != SHT_NOBITS)
        *poffset += section->size;

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

// Reset layout_complete on all sections
static void reset_layout_complete(List *input_elf_files) {
    for (int i = 0; i < input_elf_files->length; i++) {
        ElfFile *elf_file = input_elf_files->elements[i];

        // Loop over all sections
        for (int j = 0; j < elf_file->section_list->length; j++) {
            Section *input_section  = (Section *) elf_file->section_list->elements[j];
            RwSection *output_section = input_section->dst_section;
            if (!output_section) continue; // Not included
            output_section->layout_complete = 0;
        }
    }

}

// Run through linker script, group sections into program segments, determine section offsets and assign addresses to symbols in the script.
// This function is run twice. The first time to collect the symbols in assignments. The offsets and addresses may be incorrect.
// The second time when the final layout is done.
// The script does a parallel walk of the the sections output list (which may contain orphans) and the script
void layout_output_sections(RwElfFile *output_elf_file, List *input_elf_files) {
    if (DEBUG_LAYOUT) printf("------------------------------------\nLaying out executable\n");

    reset_layout_complete(input_elf_files);

    uint64_t offset = 0;

    Symbol *dot_symbol = get_or_add_linker_script_symbol(strdup("."));
    dot_symbol->dst_value = 0; // Zero indicates no offset has been set yet.

    RwSection* last_section = NULL;

    // Walk the sections list in parallel with the script to also handle orphans that have been relocated
    // to within the script's sections
    int current_sections_list_index = -1; // This will be set when the first output section is encountered in the script.

    for (int i = 0; i < linker_script->length; i++) {
        ScriptCommand *script_command = linker_script->elements[i];

        if (script_command->type != CMD_SECTIONS) continue;
        List *section_commands = script_command->sections.commands;

        for (int j = 0; j < section_commands->length; j++) {
            if (current_sections_list_index != -1) {
                // If the current section is an orphan, layout all orphans until the first non-orphan is found.
                RwSection *current = output_elf_file->sections_list->elements[current_sections_list_index];
                while (current->is_orphan) {
                    if (current->keep || current->size > 0) {
                        layout_one_section_in_executable(output_elf_file, current, last_section, dot_symbol, &offset);
                        last_section = current;
                    }

                    current_sections_list_index++;
                    if (current_sections_list_index == output_elf_file->sections_list->length) {
                        // We've reached the end of the list. There are also no more layout script commands.
                        break;
                    }

                    current = output_elf_file->sections_list->elements[current_sections_list_index];
                }
            }

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

                // Ensure the current section is the same as the one referenced in the script.
                if (current_sections_list_index != -1 && section != output_elf_file->sections_list->elements[current_sections_list_index]) {
                    RwSection *unexpected_section = output_elf_file->sections_list->elements[current_sections_list_index];
                    panic("Expected %s, got %s", section->name, unexpected_section->name);
                }

                else if (current_sections_list_index == -1) {
                    // This handles the first section script command. Advance the current section to match
                    // the section in the script.
                    // This moves past default sections like the NULL, strtab etc.
                    while (section != output_elf_file->sections_list->elements[current_sections_list_index]) {
                        current_sections_list_index++;
                        if (current_sections_list_index == output_elf_file->sections_list->length)
                            panic("Could not locate section %s from script in section list", section->name);
                    }
                }

                if (section->keep || section->size > 0) {
                    layout_one_section_in_executable(output_elf_file, section, last_section, dot_symbol, &offset);
                    last_section = section;
                }
                current_sections_list_index++;
            }

            if (current_sections_list_index == output_elf_file->sections_list->length) break;
        }
    }
}

// Check that sections have an increasing offset and don't overlap.
void check_output_sections(RwElfFile *output_elf_file) {
    // Loop over all pairs of sections
    for (int i = 1; i < output_elf_file->sections_list->length; i++) {
        for (int j = i + 1; j < output_elf_file->sections_list->length; j++) {
            RwSection *s1 = output_elf_file->sections_list->elements[i];
            RwSection *s2 = output_elf_file->sections_list->elements[j];

            if (s1->type == SHT_NOBITS) continue; // BSS sections are allowed to overlap

            uint64_t s1_end = s1->offset + s1->size;

            if (s1->offset > s2->offset) {
                dump_sections(output_elf_file);

                panic("Offsets are out of order: %s %#x and %s %#x",
                    s1->name, s1->offset,
                    s2->name, s2->offset
                );
            }

            if (s1_end > s2->offset) {
                dump_sections(output_elf_file);

                panic("Sections overlap: %s %#x - %#lx and %s %#x - %#x",
                    s1->name, s1->offset, s1_end,
                    s2->name, s2->offset, s2->offset + s2->size
                );
            }
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
    int previous_section_type = -1;
    uint64_t previous_section_offset = 0;

    // Loop over all headers
    for (int i = 0; i < output_elf_file->sections_list->length; i++) {
        RwSection *section = output_elf_file->sections_list->elements[i];
        if (section->type == SHT_NULL) continue;
        if (!(section->flags & SHF_ALLOC)) continue;

        if (!current_segment ||
            section->flags != current_segment_flags ||
            (previous_section_type == SHT_NOBITS && section->type != SHT_NOBITS)
        ) {
            // Create a new segment.

            // Either the segment is new
            // or the flags mismatch
            // or the type goes from NOBITS to non-NOBITS.
            //
            // NOBITS sections are allowed to be merged after non-NOBITS sections, but not the other way around.

            if (current_segment && current_segment->p_offset + current_segment->p_filesz > section->offset) {
                dump_sections(output_elf_file);
                dump_program_segments(output_elf_file);
                panic("Overlap in segments %d: %#lx > section %s offset %#x",
                    i, current_segment->p_offset + current_segment->p_filesz, section->name, section->offset);
            }

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

        if (section->type != SHT_NOBITS)
            previous_section_offset = section->offset + section->size;

        previous_section_type = section->type;
    }
}