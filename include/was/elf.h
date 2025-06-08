#ifndef _ELF_WAS_H
#define _ELF_WAS_H

#include "elf.h"
#include "list.h"
#include "output-elf.h"

extern OutputElfFile *output_elf_file;

// ELF file
void init_elf_file(const char *output_filename);
void finish_elf(char *filename);

#endif
