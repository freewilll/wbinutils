#ifndef _RELOCATIONS_H
#define _RELOCATIONS_H

#include "list.h"
#include "input-elf.h"
#include "output-elf.h"

#define RELOCATION_PHASE_SCAN  1  // Relax instructions where possible and determine which symbols need to be in the GOT
#define RELOCATION_PHASE_APPLY 2  // Do the relocation math on the final symbol values and write them to the output

// scan_relocation return code. Also encodes the amount of errors.
#define SCAN_RELOCATION_OK                                    0
#define SCAN_RELOCATION_NEEDS_GOT                             1
#define SCAN_RELOCATION_NEEDS_GOT_PLT                         2
#define SCAN_RELOCATION_NEEDS_R_X86_64_RELATIVE_RELOCATION    3
#define SCAN_RELOCATION_NEEDS_R_X86_64_64_RELOCATION          4

void make_global_symbols_in_use(OutputElfFile *output_elf_file, List *input_elf_files);
int scan_relocation(OutputElfFile *output_elf_file, InputElfFile *input_elf_file, InputSection *input_section, ElfRelocation *relocation);
void apply_relocation_to_output_elf_file(OutputElfFile *output_elf_file, InputElfFile *input_elf_file, InputSection *input_section, ElfRelocation *relocation);
void process_relocations(OutputElfFile *output_elf_file, List *input_elf_files, int phase);

#endif