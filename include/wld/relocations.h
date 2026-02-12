#ifndef _RELOCATIONS_H
#define _RELOCATIONS_H

#include "list.h"
#include "input-elf.h"
#include "output-elf.h"

#define RELOCATION_PHASE_SCAN  1  // Relax instructions where possible and determine which symbols need to be in the GOT
#define RELOCATION_PHASE_APPLY 2  // Do the relocation math on the final symbol values and write them to the output

static const char *RELOCATION_NAMES[] = {
    "R_X86_64_NONE",            // 0
    "R_X86_64_64",
    "R_X86_64_PC32",
    "R_X86_64_GOT32",
    "R_X86_64_PLT32",
    "R_X86_64_COPY",
    "R_X86_64_GLOB_DAT",
    "R_X86_64_JUMP_SLOT",
    "R_X86_64_RELATIVE",
    "R_X86_64_GOTPCREL",
    "R_X86_64_32",              // 10
    "R_X86_64_32S",
    "R_X86_64_16",
    "R_X86_64_PC16",
    "R_X86_64_8",
    "R_X86_64_PC8",
    "R_X86_64_DTPMOD64",
    "R_X86_64_DTPOFF64",
    "R_X86_64_TPOFF64",
    "R_X86_64_TLSGD",
    "R_X86_64_TLSLD",           // 20
    "R_X86_64_DTPOFF32",
    "R_X86_64_GOTTPOFF",
    "R_X86_64_TPOFF32",
    "R_X86_64_PC64",
    "R_X86_64_GOTOFF64",
    "R_X86_64_GOTPC32",
    "R_X86_64_GOT64",
    "R_X86_64_GOTPCREL64",
    "R_X86_64_GOTPC64",
    "R_X86_64_GOTPLT64",        // 30
    "R_X86_64_PLTOFF64",
    "R_X86_64_SIZE32",
    "R_X86_64_SIZE64",
    "R_X86_64_GOTPC32_TLSDESC",
    "R_X86_64_TLSDESC_CALL",
    "R_X86_64_TLSDESC",
    "R_X86_64_IRELATIVE",
    "R_X86_64_RELATIVE64",
    "UNKNOWN", // 39
    "UNKNOWN", // 40
    "R_X86_64_GOTPCRELX",
    "R_X86_64_REX_GOTPCRELX",
    "R_X86_64_NUM",
};

static int RELOCATION_NAMES_COUNT = sizeof(RELOCATION_NAMES) / sizeof(RELOCATION_NAMES[0]) - 1;

void make_global_symbols_in_use(OutputElfFile *output_elf_file, List *input_elf_files);
void scan_relocation(OutputElfFile *output_elf_file, InputElfFile *input_elf_file, InputSection *input_section, ElfRelocation *relocation);
void apply_relocation(OutputElfFile *output_elf_file, InputElfFile *input_elf_file, InputSection *input_section, ElfRelocation *relocation);
void process_relocations(OutputElfFile *output_elf_file, List *input_elf_files, int phase);

#endif