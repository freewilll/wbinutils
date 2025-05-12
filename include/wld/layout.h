#ifndef _LAYOUT_H
#define _LAYOUT_H

void layout_input_sections(RwElfFile *output_elf_file, List *input_elf_files);
void layout_leftover_sections(RwElfFile *output_elf_file, List *input_elf_files);
uint64_t layout_output_sections(RwElfFile *output_elf_file);
void layout_leftover_output_sections(RwElfFile *output_elf_file, List *input_elf_files, uint64_t offset);
void remove_empty_sections(RwElfFile *output_elf_file);
void make_elf_section_headers(RwElfFile *output_elf_file);
void layout_program_segments(RwElfFile *output_elf_file);

#endif
