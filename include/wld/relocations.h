#ifndef _RELOCATIONS_H
#define _RELOCATIONS_H

#include "list.h"
#include "rw-elf.h"


int apply_relocation(ElfRelocation *relocation, uint32_t output_virtual_address, uint64_t value, void *output_pointer, uint64_t output_offset);
void apply_relocations(List *input_elf_files, RwElfFile *output_elf_file);

#endif