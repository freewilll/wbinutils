#ifndef _RELOCATIONS_H
#define _RELOCATIONS_H

#include "list.h"
#include "rw-elf.h"

#define RELOCATION_PHASE_SCAN  1  // Relax instructions where possible and determine which symbols need to be in the GOT
#define RELOCATION_PHASE_APPLY 2  // Do the relocation math on the final symbol values and write them to the output

// scan_relocation return code. Also encodes the amount of errors.
#define SCAN_RELOCATION_OK        0
#define SCAN_RELOCATION_ERROR     1
#define SCAN_RELOCATION_NEEDS_GOT 2

int make_global_symbols_in_use(RwElfFile *output_elf_file, List *input_elf_files);
int scan_relocation(void *input_data, ElfRelocation *relocation);
int apply_relocation(RwElfFile *output_elf_file, void *output_pointer, uint64_t rw_section_offset, uint64_t rw_section_address, uint64_t output_offset, ElfRelocation *relocation, uint64_t value, int is_tls_value, uint64_t value_got_offset, uint64_t value_iplt_offset, uint64_t value_got_iplt_offset);
void apply_relocations(List *input_elf_files, RwElfFile *output_elf_file, int phase);

#endif