#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "elf.h"
#include "error.h"
#include "list.h"
#include "ro-elf.h"

#include "wld/symbols.h"
#include "wld/relocations.h"
#include "wld/wld.h"

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

int RELOCATION_NAMES_COUNT = sizeof(RELOCATION_NAMES) / sizeof(RELOCATION_NAMES[0]) - 1;

void apply_relocations(List *input_elf_files, RwElfFile *output_elf_file) {
    // While in development, collect all problems, report them and bail.
    // This way I know what lies ahead until the executable can be run.
    int undefined_symbols = 0;
    int failed_relocations = 0;

    if (DEBUG) printf("\nRelocations:\n");

    // Loop over all input files
    for (int i = 0; i < input_elf_files->length; i++) {
        ElfFile *input_elf_file = input_elf_files->elements[i];

        if (DEBUG) printf("%s\n", input_elf_file->filename);

        // Loop over all relocation sections
        for (int j = 0; j < input_elf_file->section_list->length; j++) {
            Section *rela_input_section  = (Section *) input_elf_file->section_list->elements[j];
            ElfSectionHeader *rela_input_elf_section_header = rela_input_section->elf_section_header;
            if (rela_input_elf_section_header->sh_type != SHT_RELA) continue;

            int target_section_index = rela_input_elf_section_header->sh_info;
            Section *input_section  = (Section *) input_elf_file->section_list->elements[target_section_index];

            if (!EXECUTABLE_SECTION_TYPE(input_section->elf_section_header->sh_type)) continue;

            // Loop over all relocations
            ElfRelocation *relocations = load_section(input_elf_file, j);
            ElfRelocation *relocation = relocations;
            ElfRelocation *end = ((void *) relocations) + rela_input_elf_section_header->sh_size;
            while (relocation < end) {
                int type = relocation->r_info & 0xffffffff;
                int symbol_index = relocation->r_info >> 32;

                // Get the symbol
                ElfSymbol *elf_symbol = &input_elf_file->symbol_table[symbol_index];
                int elf_symbol_type = elf_symbol->st_info & 0xf;

                int dst_value;
                char *symbol_name = NULL;

                // Get the output section
                RwSection *rw_section = get_rw_section(output_elf_file, input_section->name);
                if (!rw_section) panic("Unexpected null section in output when applying relocations");

                if (elf_symbol_type == STT_SECTION) {
                    // Handle a relocation to a sectio symbol

                    Section *symbol_section =  (Section *) input_elf_file->section_list->elements[elf_symbol->st_shndx];
                    symbol_name = symbol_section->name;

                    RwSection *symbol_rw_section = get_rw_section(output_elf_file, symbol_section->name);
                    if (!symbol_rw_section) panic("Unexpected null section in output when applying relocations");

                    dst_value = output_elf_file->executable_virt_address + symbol_rw_section->offset + elf_symbol->st_value;
                }
                else {
                    // Handle a relocation to a non-section symbol

                    symbol_name = &input_elf_file->strtab_strings[elf_symbol->st_name];

                    Symbol *symbol = lookup_symbol(input_elf_file, symbol_name);
                    if (!symbol) {
                        if (!is_undefined_symbol(symbol_name)) {
                            printf("Trying to relocate a symbol that's not defined: %s in section %s\n",
                                symbol_name, input_section->name);
                            undefined_symbols++;
                        }

                        // It's a weak symbol; they are allowed to be undefined. Their value defaults to zero.
                        dst_value = 0;
                    } else {
                        dst_value = symbol->dst_value;
                    }
                }

                if (DEBUG) {
                    const char *relocation_name = type < RELOCATION_NAMES_COUNT ? RELOCATION_NAMES[type] : "UNKNOWN";
                    printf("  input section %s, rel=%s, offset %#08lx,  %s + %ld\n",
                        input_section->name, relocation_name, relocation->r_offset, symbol_name, relocation->r_addend);
                }

                // When linking statically, a R_X86_64_PLT32 is treated like a R_X86_64_PC32
                if (type == R_X86_64_PLT32) type = R_X86_64_PC32;

                uint64_t offset_in_rw_section = input_section->offset + relocation->r_offset;

                uint32_t A = relocation->r_addend;
                uint32_t P = output_elf_file->executable_virt_address + rw_section->offset + offset_in_rw_section;
                uint32_t S = dst_value;

                if (DEBUG) printf("    S=%#x P=%#x A=%#x\n", S, P, A);

                switch (type) {
                    case R_X86_64_64: {
                        uint64_t *output = (uint64_t *) (rw_section->data + offset_in_rw_section);
                        uint64_t value = S + A;
                        if (DEBUG) printf("    value=%#lx\n", value);
                        *output = value;
                        break;
                    }

                    case R_X86_64_PC32: {
                        uint32_t *output = (uint32_t *) (rw_section->data + offset_in_rw_section);
                        uint32_t value = S + A - P;
                        if (DEBUG) printf("    value=%#x\n", value);
                        *output = value;
                        break;
                    }

                    case R_X86_64_32:
                    case R_X86_64_32S: {
                        uint32_t value = S + A;
                        uint32_t *output = (uint32_t *) (rw_section->data + offset_in_rw_section);
                        if (DEBUG) printf("    value=%#x\n", value);
                        *output = value;
                        break;
                    }
                    case R_X86_64_GOTPCRELX: {
                        // Relax instructions to not use the GOT

                        uint32_t *output = (uint32_t *) (rw_section->data + offset_in_rw_section);

                        uint8_t *mod_rm = (uint8_t *) (rw_section->data + offset_in_rw_section - 1);
                        uint8_t *popcode = (uint8_t *) (rw_section->data + offset_in_rw_section - 2);
                        uint8_t opcode = *popcode;

                        if (opcode == 0x8b) {
                            // Convert movl foo@GOTPCREL(%rip), %eax to mov $foo,%eax
                            *popcode = 0xc7;
                            *mod_rm = 0xc0 | (*mod_rm & 0x38) >> 3;

                            uint32_t value = S;
                            if (DEBUG) printf("    value=%#x\n", value);
                            *output = value;
                            break;
                        }

                        else if (opcode == 0xff) {
                            if (*mod_rm == 0x15) {
                                // Convert callq  foo(%rip) to addr32 callq foo
                                *popcode = 0x67;
                                *mod_rm = 0xe8;
                            }
                            else
                                panic("Unhandled instruction rewrite for R_X86_64_REX_GOTP: %#02x %#02x\n", opcode, *mod_rm);

                            uint32_t value = S + A - P;
                            if (DEBUG) printf("    value=%#x\n", value);
                            *output = value;
                            break;
                        }

                        panic("Unhandled instruction rewrite for R_X86_64_GOTPCRELX: %#x\n", opcode);
                    }

                    case R_X86_64_REX_GOTP: {
                        // Relax instructions to not use the GOT

                        uint32_t *output = (uint32_t *) (rw_section->data + offset_in_rw_section);

                        uint8_t *pprefix = (uint8_t *) (rw_section->data + offset_in_rw_section - 3);
                        uint8_t *popcode = (uint8_t *) (rw_section->data + offset_in_rw_section - 2);
                        uint8_t *mod_rm = (uint8_t *) (rw_section->data + offset_in_rw_section - 1);
                        uint8_t opcode = *popcode;

                        if (opcode == 0x8b) {
                            // Convert movq foo@GOTPCREL(%rip), %rax to movq $foo,%rax

                            // Clear REX W
                            if (*pprefix == 0x4c)
                                *pprefix = 0x49;

                            *popcode = 0xc7;
                            *mod_rm = 0xc0 | (*mod_rm >> 3);

                            uint32_t value = S;
                            if (DEBUG) printf("    value=%#x\n", value);
                            *output = value;
                            break;
                        }

                        printf("Unhandled instruction rewrite for R_X86_64_REX_GOTP: %#02x %#02x\n", *pprefix, opcode);
                        failed_relocations++;

                        break;
                    }

                    default: {
                        const char *relocation_name = type < RELOCATION_NAMES_COUNT ? RELOCATION_NAMES[type] : "UNKNOWN";
                        printf("Unhandled relocation type %s\n", relocation_name);
                        failed_relocations++;
                    }
                }

                relocation++;
            }

            free(relocations);
        }
    }

    // Bail if there are any issues
    if (undefined_symbols || failed_relocations) {
        printf("Failed to apply relocations. Undefined symbols: %d, failed relocations: %d\n", undefined_symbols, failed_relocations);
        exit(1);
    }
}
