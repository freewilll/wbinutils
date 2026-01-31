#ifndef _RELOCATIONS_H
#define _RELOCATIONS_H

#include "list.h"
#include "input-elf.h"
#include "output-elf.h"

#define RELOCATION_PHASE_SCAN  1  // Relax instructions where possible and determine which symbols need to be in the GOT
#define RELOCATION_PHASE_APPLY 2  // Do the relocation math on the final symbol values and write them to the output

void make_global_symbols_in_use(OutputElfFile *output_elf_file, List *input_elf_files);
void scan_relocation(OutputElfFile *output_elf_file, InputElfFile *input_elf_file, InputSection *input_section, ElfRelocation *relocation);
void apply_relocation(OutputElfFile *output_elf_file, InputElfFile *input_elf_file, InputSection *input_section, ElfRelocation *relocation);
void process_relocations(OutputElfFile *output_elf_file, List *input_elf_files, int phase);

#endif