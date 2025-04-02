#ifndef _RELOCATIONS_H
#define _RELOCATIONS_H

#include "list.h"
#include "rw-elf.h"

void apply_relocations(List *input_elf_files, RwElfFile *output_elf_file);

#endif