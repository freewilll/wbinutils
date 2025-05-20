#ifndef _LAYOUT_H
#define _LAYOUT_H

#include "wld/script.h"

// Assigmnent command, used in sections and sections output
typedef struct output_section_assignment  {
    CommandAssignment *assignment;  // Linker script assignment
    uint64_t offset;                // Offset into the output section
} OutputSectionAssignment;

void layout_input_sections(RwElfFile *output_elf_file, List *input_elf_files);
void layout_leftover_sections(RwElfFile *output_elf_file, List *input_elf_files);
uint64_t layout_output_sections(RwElfFile *output_elf_file);
void layout_leftover_output_sections(RwElfFile *output_elf_file, List *input_elf_files, uint64_t offset);
void remove_empty_sections(RwElfFile *output_elf_file);
void make_elf_section_headers(RwElfFile *output_elf_file);
void make_output_section_command_assignments_symbol_values(RwElfFile *output_elf_file);
void layout_program_segments(RwElfFile *output_elf_file);

#endif
