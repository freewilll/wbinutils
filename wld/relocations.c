#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "error.h"
#include "list.h"
#include "ro-elf.h"

#include "wld/symbols.h"
#include "wld/relocations.h"
#include "wld/wld.h"

void apply_relocations(List *input_elf_files, RwElfFile *output_elf_file) {
    if (DEBUG) printf("\nRelocations:\n");

    // Loop over all input files
    for (int i = 0; i < input_elf_files->length; i++) {
        ElfFile *input_elf_file = input_elf_files->elements[i];

        // Loop over all relocation sections
        for (int j = 0; j < input_elf_file->section_list->length; j++) {
            Section *rela_input_section  = (Section *) input_elf_file->section_list->elements[j];
            ElfSectionHeader *rela_input_elf_section_header = rela_input_section->elf_section_header;
            if (rela_input_elf_section_header->sh_type != SHT_RELA) continue;

            int target_section_index = rela_input_elf_section_header->sh_info;
            Section *input_section  = (Section *) input_elf_file->section_list->elements[target_section_index];

            // Loop over all relocations
            ElfRelocation *relocations = load_section(input_elf_file, j);
            ElfRelocation *relocation = relocations;
            ElfRelocation *end = ((void *) relocations) + rela_input_elf_section_header->sh_size;
            while (relocation < end) {
                int type = relocation->r_info & 0xffffffff;
                int symbol_index = relocation->r_info >> 32;

                // Get the symbol
                ElfSymbol *elf_symbol = &input_elf_file->symbol_table[symbol_index];
                char *symbol_name = &input_elf_file->strtab_strings[elf_symbol->st_name];
                Symbol *symbol = get_defined_symbol(symbol_name);

                if (DEBUG) {
                    printf("  offset %#08lx type=%d %s + %ld\n", relocation->r_offset, type, symbol_name, relocation->r_addend);
                    printf("    input_section->offset=%#x\n", input_section->offset);
                }

                // Get the output section
                RwSection *rw_section = get_rw_section(output_elf_file, input_section->name);

                // When linking statically, a R_X86_64_PLT32 is treated like a R_X86_64_PC32
                if (type == R_X86_64_PLT32) type = R_X86_64_PC32;

                uint64_t offset_in_rw_section = input_section->offset + relocation->r_offset;

                uint32_t A = relocation->r_addend;
                uint32_t P = output_elf_file->executable_virt_address + rw_section->offset + offset_in_rw_section;
                uint32_t S = symbol->dst_value;

                if (DEBUG) printf("    S=%#x P=%#x A=%#x\n", S, P, A);

                uint32_t *output = (uint32_t *) (rw_section->data + offset_in_rw_section);

                switch (type) {
                    case R_X86_64_PC32: {
                        uint32_t value = S + A - P;
                        if (DEBUG) printf("    value=%#x\n", value);
                        *output = value;
                        break;
                    }

                    case R_X86_64_32:
                    case R_X86_64_32S: {
                        uint32_t value = S + A;
                        if (DEBUG) printf("    value=%#x\n", value);
                        *output = value;
                        break;
                    }

                    default:
                        panic("Unhandled relocation type %d", type);
                }

                relocation++;
            }

            free(relocations);
        }
    }
}
