#ifndef _RELOCATIONS_H
#define _RELOCATIONS_H

#include "list.h"
#include "rw-elf.h"


int apply_relocation(RwElfFile *output_elf_file, void *output_pointer, uint64_t rw_section_offset, uint64_t output_offset, ElfRelocation *relocation, uint64_t value, int is_tls_value);
void apply_relocations(List *input_elf_files, RwElfFile *output_elf_file);

#endif