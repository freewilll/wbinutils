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

#define VERBOSE_ERROR_LIST 0

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

// mod_rem encoding and decoding
#define MOD_RM_MODE_REGISTER_DIRECT_ADDRESSING 3

#define ENCODE_MOD_RM(mod, reg, rm) ((((mod) & 3) << 6) | (((reg) & 7) << 3) | ((rm) & 7))
#define DECODE_MOD_RM_MODE(mod_rm) ((mod_rm) >> 6)
#define DECODE_MOD_RM_REG(mod_rm) (((mod_rm) >> 3) & 7)
#define DECODE_MOD_RM_RM(mod_rm) ((mod_rm) & 7)

int RELOCATION_NAMES_COUNT = sizeof(RELOCATION_NAMES) / sizeof(RELOCATION_NAMES[0]) - 1;

// Convert an opcode where the first operand is in G encoding to E encoding
// G: The reg field of the ModR/M byte selects a general register
// E: A ModR/M byte follows the opcode and specifies the operand. The operand is either a general-purpose register or a memory address.
void convert_Gvqp_to_Evqp(void *output_pointer, uint8_t opcode, uint8_t binary_operation, uint32_t value) {
    uint32_t *output = (uint32_t *) output_pointer;
    uint8_t *pprefix = (uint8_t *) (output_pointer - 3);
    uint8_t *popcode = (uint8_t *) (output_pointer - 2);
    uint8_t *mod_rm = (uint8_t *) (output_pointer - 1);

    int mod_r = (*pprefix >> 2) & 1;        // The high bit of the register is in REX W (bit 2)
    *pprefix = 0x48 | mod_r;                // The high bit of the register is in REX B (bit 0)
    *popcode = opcode;

    // Move the register from reg to rm. reg = binary_operation
    *mod_rm = ENCODE_MOD_RM(MOD_RM_MODE_REGISTER_DIRECT_ADDRESSING, binary_operation, DECODE_MOD_RM_REG(*mod_rm));

    if (DEBUG) printf("    value=%#x\n", value);
    *output = value;
}

// This function rewrites the output code. All ELF file details are abstracted away, so that function can be easily tested.
int apply_relocation(RwElfFile *output_elf_file, void *output_pointer, uint64_t rw_section_offset, uint64_t output_offset, ElfRelocation *relocation, uint64_t value, int is_tls_value) {
    uint32_t output_virtual_address = output_elf_file->executable_virt_address + rw_section_offset + output_offset;

    output_pointer += output_offset;
    int type = relocation->r_info & 0xffffffff;

    uint32_t A = relocation->r_addend;
    uint32_t P = output_virtual_address;
    uint32_t S = value;

    // When linking statically, a R_X86_64_PLT32 is treated like a R_X86_64_PC32
    if (type == R_X86_64_PLT32) type = R_X86_64_PC32;

    if (type == R_X86_64_GOTTPOFF) {
        // Convert a foo@GOTTPOFF(%rip) to foo@GOTPCREL(%rip)
        type = R_X86_64_REX_GOTPCRELX;
        S -= output_elf_file->tls_template_size;
    }

    if (DEBUG) printf("    S=%#x P=%#x A=%#x\n", S, P, A);

    switch (type) {
        case R_X86_64_64: {
            uint64_t *output = (uint64_t *) output_pointer;
            uint64_t value = S + A;
            if (DEBUG) printf("    value=%#lx\n", value);
            *output = value;
            break;
        }

        case R_X86_64_PC32: {
            uint32_t *output = (uint32_t *) output_pointer;
            uint32_t value = S + A - P;

            // Unusual case of accessing a TLS template variable directly.
            // This is mostly to make an unusual TLS test case work.
            if (is_tls_value) value += output_elf_file->tls_template_virt_address;

            if (DEBUG) printf("    value=%#x\n", value);
            *output = value;
            break;
        }

        case R_X86_64_32:
        case R_X86_64_32S: {
            uint32_t value = S + A;
            uint32_t *output = (uint32_t *) output_pointer;
            if (DEBUG) printf("    value=%#x\n", value);
            *output = value;
            break;
        }
        case R_X86_64_GOTPCRELX: {
            // Relax instructions to not use the GOT

            uint32_t *output = (uint32_t *) output_pointer;

            uint8_t *pmod_rm = (uint8_t *) (output_pointer - 1);
            uint8_t *popcode = (uint8_t *) (output_pointer - 2);
            uint8_t *pprefix = (uint8_t *) (output_pointer - 3);
            uint8_t opcode = *popcode;

            if (output_offset > 1 && opcode == 0x8b) {
                // Convert movl foo@GOTPCREL(%rip), %eax to mov $foo, %eax
                *popcode = 0xc7;
                *pmod_rm = 0xc0 | (*pmod_rm & 0x38) >> 3;

                uint32_t value = S; // Ignore the addend, this is an absolute address
                if (DEBUG) printf("    value=%#x\n", value);
                *output = value;
                break;
            }

            else if (output_offset > 1 && opcode == 0xff) {
                if (*pmod_rm == 0x15) {
                    // Convert callq foo(%rip) to addr32 callq foo
                    *popcode = 0x67;
                    *pmod_rm = 0xe8;
                }
                else
                    panic("Unhandled instruction rewrite for R_X86_64_GOTPCRELX: %#02x %#02x\n", opcode, *pmod_rm);

                uint32_t value = S + A - P;
                if (DEBUG) printf("    value=%#x\n", value);
                *output = value;
                break;
            }

            panic("Unhandled instruction rewrite for R_X86_64_GOTPCRELX: %#x\n", opcode);
        }

        case R_X86_64_GOTPCREL:
        case R_X86_64_REX_GOTPCRELX: {
            // Relax instructions to not use the GOT

            uint32_t *output = (uint32_t *) output_pointer;

            uint8_t *pprefix = (uint8_t *) (output_pointer - 3);
            uint8_t *popcode = (uint8_t *) (output_pointer - 2);
            uint8_t opcode = *popcode;

            if (output_offset > 2 && opcode == 0x8b) {
                // Convert movq foo@GOTPCREL(%rip), %rax to movq $foo, %rax
                convert_Gvqp_to_Evqp(output_pointer, 0xc7, 0, S);
                break;
            }

            else if (output_offset > 2 && (*pprefix == 0x48 || *pprefix == 0x4c) && opcode == 0x3b) {
                // Convert cmpq foo@GOTPCREL(%rip), %rax to cmpq foo, %rax
                convert_Gvqp_to_Evqp(output_pointer, 0x81, 7, S);
                break;
            }

            else if (output_offset > 2 && (*pprefix == 0x48 || *pprefix == 0x4c) && opcode == 0x2b) {
                // Convert subq foo@GOTPCREL(%rip), %rax to subq foo, %rax
                convert_Gvqp_to_Evqp(output_pointer, 0x81, 5, S);
                break;
            }

            if (VERBOSE_ERROR_LIST)
                printf("Unhandled instruction rewrite for R_X86_64_REX_GOTP: %#02x %#02x\n", *pprefix, opcode);
            return 1;
        }

        case R_X86_64_TPOFF32: {
            uint32_t *output = (uint32_t *) output_pointer;
            uint32_t value = S + A - output_elf_file->tls_template_size;
            if (DEBUG) printf("    value=%#x\n", value);
            *output = value;
            break;
        }

        default: {
            const char *relocation_name = type < RELOCATION_NAMES_COUNT ? RELOCATION_NAMES[type] : "UNKNOWN";
            if (VERBOSE_ERROR_LIST)
                printf("Unhandled relocation type %s\n", relocation_name);
            return 1;
        }
    }

    return 0;
}

// Given an input file, output file, input section and relocation, process the relocation by modifying the output.
int apply_relocation_with_elf_files(RwElfFile *output_elf_file, ElfFile *input_elf_file, Section *input_section, ElfRelocation *relocation) {
    int type = relocation->r_info & 0xffffffff;
    int symbol_index = relocation->r_info >> 32;

    // Get the symbol
    ElfSymbol *elf_symbol = &input_elf_file->symbol_table[symbol_index];
    int elf_symbol_type = elf_symbol->st_info & 0xf;

    int dst_value;
    char *symbol_name = NULL;
    int is_tls_value = 0;

    // Get the output section
    RwSection *rw_section = get_rw_section(output_elf_file, input_section->name);
    if (!rw_section) panic("Unexpected null section in output when applying relocations");

    // Determine the value of the symbol
    if (elf_symbol_type == STT_SECTION) {
        // Handle a relocation to a section symbol

        Section *symbol_section = (Section *) input_elf_file->section_list->elements[elf_symbol->st_shndx];
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
            // It's a weak symbol; they are allowed to be undefined. Their value defaults to zero.
            dst_value = 0;
        } else {
            dst_value = symbol->dst_value;
            is_tls_value = symbol->type == STT_TLS;
        }
    }

    if (DEBUG) {
        const char *relocation_name = type < RELOCATION_NAMES_COUNT ? RELOCATION_NAMES[type] : "UNKNOWN";
        printf("  input section %s, rel=%s, offset %#08lx,  %s + %ld\n",
            input_section->name, relocation_name, relocation->r_offset, symbol_name, relocation->r_addend);
    }

    uint64_t output_offset = input_section->offset + relocation->r_offset;

    return apply_relocation(output_elf_file, rw_section->data, rw_section->offset, output_offset, relocation, dst_value, is_tls_value);
}

void apply_relocations(List *input_elf_files, RwElfFile *output_elf_file) {
    // While in development, collect all problems, report them and bail.
    // This way I know what lies ahead until the executable can be run.
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
                failed_relocations += apply_relocation_with_elf_files(output_elf_file, input_elf_file, input_section, relocation);
                relocation++;
            }

            free(relocations);
        }
    }

    // Bail if there are any issues
    if (failed_relocations) {
        printf("Failed to apply relocations: failed relocations: %d\n", failed_relocations);
        exit(1);
    }
}
