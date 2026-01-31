#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "elf.h"
#include "error.h"
#include "list.h"
#include "input-elf.h"

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

typedef enum got_or_plt_relocation_type {
    RT_GOT,
    RT_PLT
} GotOrPltRelocationType;

// mod_rem encoding and decoding
#define MOD_RM_MODE_REGISTER_DIRECT_ADDRESSING 3

#define ENCODE_MOD_RM(mod, reg, rm) ((((mod) & 3) << 6) | (((reg) & 7) << 3) | ((rm) & 7))
#define DECODE_MOD_RM_MODE(mod_rm) ((mod_rm) >> 6)
#define DECODE_MOD_RM_REG(mod_rm) (((mod_rm) >> 3) & 7)
#define DECODE_MOD_RM_RM(mod_rm) ((mod_rm) & 7)

int RELOCATION_NAMES_COUNT = sizeof(RELOCATION_NAMES) / sizeof(RELOCATION_NAMES[0]) - 1;

// Make set of all global symbols that have relocations
void make_global_symbols_in_use(OutputElfFile *output_elf_file, List *input_elf_files) {
    StrMap *global_symbols_in_use = output_elf_file->global_symbols_in_use;

    // Loop over all input files
    for (int i = 0; i < input_elf_files->length; i++) {
        InputElfFile *input_elf_file = input_elf_files->elements[i];

        // Loop over all relocation sections
        for (int j = 0; j < input_elf_file->section_list->length; j++) {
            InputSection *rela_input_section  = (InputSection *) input_elf_file->section_list->elements[j];
            if (rela_input_section->type != SHT_RELA) continue;

            int target_section_index = rela_input_section->info;
            InputSection *input_section  = (InputSection *) input_elf_file->section_list->elements[target_section_index];

            int link = rela_input_section->link;
            InputSection *symbol_section = input_elf_file->section_list->elements[rela_input_section->link];
            load_section(input_elf_file, symbol_section);

            InputSection *str_section = input_elf_file->section_list->elements[symbol_section->link];
            char *strings = load_section(input_elf_file, str_section);

            // Loop over all relocations
            ElfRelocation *relocations = load_section_uncached(input_elf_file, j);
            ElfRelocation *relocation = relocations;
            ElfRelocation *end = ((void *) relocations) + rela_input_section->size;

            while (relocation < end) {
                int type = relocation->r_info & 0xffffffff;
                int symbol_index = relocation->r_info >> 32;

                // Ignore relocations that don't refer to symbols
                if (type == R_X86_64_RELATIVE) {
                    relocation++;
                    continue;
                }

                // Get the symbol
                ElfSymbol *elf_symbol = &((ElfSymbol *) symbol_section->data)[symbol_index];
                int elf_symbol_type = elf_symbol->st_info & 0xf;
                char elf_symbol_binding = (elf_symbol->st_info >> 4) & 0xf;

                // Relocatable things can be of STT_NOTYPE, so are included here
                int is_relocatable_type = elf_symbol_type == STT_NOTYPE || elf_symbol_type == STT_OBJECT || elf_symbol_type == STT_FUNC;

                // Weak bindings are allowed to be undefined so are not considered in use
                if (is_relocatable_type && elf_symbol_binding != STB_WEAK) {
                    char *symbol_name = &strings[elf_symbol->st_name];
                    // The symbol may be undefined, which is an error that will be reported later.

                    // Add it to the symbols in use map
                    if (!strmap_get(global_symbols_in_use, symbol_name)) strmap_put(global_symbols_in_use, symbol_name, (void *) 1);
                }

                relocation++;
            }

            free(relocations);
        }
    }
}

// Convert an opcode where the first operand is in G encoding to E encoding
// G: The reg field of the ModR/M byte selects a general register
// E: A ModR/M byte follows the opcode and specifies the operand. The operand is either a general-purpose register or a memory address.
static void convert_Gvqp_to_Evqp(void *data, uint8_t opcode, uint8_t binary_operation) {
    uint8_t *pprefix = (uint8_t *) (data - 3);
    uint8_t *popcode = (uint8_t *) (data - 2);
    uint8_t *mod_rm = (uint8_t *) (data - 1);

    int mod_r = (*pprefix >> 2) & 1;        // The high bit of the register is in REX W (bit 2)
    *pprefix = 0x48 | mod_r;                // The high bit of the register is in REX B (bit 0)
    *popcode = opcode;

    // Move the register from reg to rm. reg = binary_operation
    *mod_rm = ENCODE_MOD_RM(MOD_RM_MODE_REGISTER_DIRECT_ADDRESSING, binary_operation, DECODE_MOD_RM_REG(*mod_rm));
}

// Lookup a symbol in the symbol table for a relocation. If null, then the relocation refers to a section..
static Symbol *get_symbol_from_relocation(InputElfFile *input_elf_file, ElfRelocation *relocation) {
    int symbol_index = relocation->r_info >> 32;
    ElfSymbol *elf_symbol = &input_elf_file->symbol_table[symbol_index];
    char *symbol_name = &input_elf_file->symbol_table_strings[elf_symbol->st_name];
    int version_index = 0;
    Symbol *symbol = lookup_symbol(input_elf_file, symbol_name, version_index);
}

static int link_symbol_dynamically(OutputElfFile *output_elf_file, Symbol *symbol) {
    int symbol_is_from_shared_library = 0;
    if (symbol && symbol->src_elf_file) symbol_is_from_shared_library = symbol->src_elf_file->type == ET_DYN;

    // Symbols in an executable cannot be preempted/interspersed, like possible in a shared library, so link them statically when possible.
    int link_dynamically = symbol_is_from_shared_library;
    if (!symbol_is_from_shared_library && output_elf_file->type == ET_DYN && !output_elf_file->is_executable) link_dynamically = 1;

    return link_dynamically;
}

// A .got or .got.plt entry is needed
static void add_got_or_plt_relocation(InputElfFile *input_elf_file, ElfRelocation *relocation, GotOrPltRelocationType add_got) {
    int type = relocation->r_info & 0xffffffff;
    int symbol_index = relocation->r_info >> 32;

    // Get the symbol
    ElfSymbol *elf_symbol = &input_elf_file->symbol_table[symbol_index];
    int elf_symbol_type = elf_symbol->st_info & 0xf;

    if (elf_symbol_type == STT_SECTION)
        panic("Handling of .got.* entries that refer to a section symbol not handled");

    if (elf_symbol_type == STT_GNU_IFUNC) {
        // This symbol will get an entry in .got.iplt. There is no need to create one in the .got.
        return;
    }

    // Lookup the symbol in the symbol table
    char *symbol_name = &input_elf_file->symbol_table_strings[elf_symbol->st_name];
    int version_index = 0;
    Symbol *symbol = lookup_symbol(input_elf_file, symbol_name, version_index);
    if (!symbol) panic("Cannot process a relocation for an undefined symbol: %s\n", symbol_name);

    if (add_got == RT_GOT) {
        symbol->extra = SE_IN_GOT;
        symbol->needs_dynsym_entry = 1;
    }
    else {
        symbol->extra = SE_IN_GOT_PLT;
        symbol->needs_dynsym_entry = 1;
    }
}

static void add_R_X86_64_RELATIVE_relocation(OutputElfFile *output_elf_file, InputElfFile *input_elf_file, InputSection *input_section, ElfRelocation *relocation) {
    // Ignore relocations for non-loadable sections, e.g. dwarf sections
    if (!(input_section->flags & SHF_ALLOC)) return;

    // A .rela.dyn entry is needed
    int symbol_index = relocation->r_info >> 32;
    ElfSymbol *elf_symbol = &input_elf_file->symbol_table[symbol_index];
    char *symbol_name = &input_elf_file->symbol_table_strings[elf_symbol->st_name];

    InputSection *relocation_input_section = NULL;
    int is_common = elf_symbol->st_shndx == SHN_COMMON;

    int version_index = 0;
    Symbol *symbol = get_defined_symbol(global_symbol_table, symbol_name, version_index);

    if (!is_common && elf_symbol->st_shndx >= SHN_LORESERVE)
        panic("Unhandled section index %d when processing", elf_symbol->st_shndx);
    else if (is_common) {
        // The symbol must be defined. This is the only way to get it's offset, allocated in the .bss section.
        if (!symbol) panic("Expected symbol to be defined due to a relocation referencing it");
    }
    else {
        // The symbol may be unset, in which case the relocation is an offset into the relocation input section.
        relocation_input_section = (InputSection *) input_elf_file->section_list->elements[elf_symbol->st_shndx];
    }

    RelativeRelaDynRelocation *rrdr = calloc(1,sizeof(RelativeRelaDynRelocation));
    rrdr->target_section = input_section;
    rrdr->symbol = symbol;
    rrdr->relocation_input_section = relocation_input_section;
    rrdr->offset = relocation->r_offset;
    rrdr->addend = relocation->r_addend;

    append_to_list(output_elf_file->rela_dyn_R_X86_64_RELATIVE_relocations, rrdr);
}

static void add_SCAN_RELOCATION_NEEDS_R_X86_64_64_relocation(OutputElfFile *output_elf_file, InputElfFile *input_elf_file, InputSection *input_section, ElfRelocation *relocation) {
    // Ignore relocations for non-loadable sections, e.g. dwarf sections
    if (!(input_section->flags & SHF_ALLOC)) return;

    // A .rela.dyn entry is needed
    int symbol_index = relocation->r_info >> 32;
    ElfSymbol *elf_symbol = &input_elf_file->symbol_table[symbol_index];
    char *symbol_name = &input_elf_file->symbol_table_strings[elf_symbol->st_name];

    InputSection *relocation_input_section = NULL;

    int version_index = 0;
    Symbol *symbol = get_defined_symbol(global_symbol_table, symbol_name, version_index);
    if (!symbol) panic("Expected %s to be defined due to a relocation referencing it", symbol_name);
    symbol->needs_dynsym_entry = 1;

    RelativeRelaDynRelocation *rrdr = calloc(1,sizeof(RelativeRelaDynRelocation));
    rrdr->target_section = input_section;
    rrdr->symbol = symbol;
    rrdr->relocation_input_section = relocation_input_section;
    rrdr->offset = relocation->r_offset;
    rrdr->addend = relocation->r_addend;

    append_to_list(output_elf_file->rela_dyn_R_X86_64_64_relocations, rrdr);
}

// Given an input file and a relocation, relax any instructions where possible and determine if the symbol needs to be in the GOT.
// The instructions are rewritten in the loaded input ELF section.
// apply_relocation() puts the relocated values in the output ELF file.
void scan_relocation(OutputElfFile *output_elf_file, InputElfFile *input_elf_file, InputSection *input_section, ElfRelocation *relocation) {
    load_section(input_elf_file, input_section);

    Symbol *symbol = get_symbol_from_relocation(input_elf_file, relocation);
    char *symbol_name = symbol ? symbol->name : NULL;

    int symbol_is_from_shared_library = 0;
    if (symbol && symbol->src_elf_file) symbol_is_from_shared_library = symbol->src_elf_file->type == ET_DYN;

    int link_dynamically = link_symbol_dynamically(output_elf_file, symbol);

    int output_is_shared = output_elf_file->type == ET_DYN;

    int type = relocation->r_info & 0xffffffff;
    uint64_t offset = relocation->r_offset;

    char *input_data = input_section->data;
    input_data += offset;

    int is_executable = output_elf_file->is_executable;

    // When linking statically, a R_X86_64_PLT32 is treated like a R_X86_64_PC32
    if (type == R_X86_64_PLT32 && !link_dynamically) type = R_X86_64_PC32;

    if (type == R_X86_64_GOTTPOFF) {
        // Convert a foo@GOTTPOFF(%rip) to foo@GOTPCREL(%rip)
        type = R_X86_64_REX_GOTPCRELX;
    }

    switch (type) {
        case R_X86_64_64:
            if (output_is_shared) {
                if (symbol_is_from_shared_library || !is_executable)
                    add_SCAN_RELOCATION_NEEDS_R_X86_64_64_relocation(output_elf_file, input_elf_file, input_section, relocation);
                else
                    add_R_X86_64_RELATIVE_relocation(output_elf_file, input_elf_file, input_section, relocation);
            }

            return;

        case R_X86_64_PC32:
            // A R_X86_64_PC32 is not a GOT-relative relocation and may not be used when making a shared library.
            if (output_is_shared && !is_executable && symbol_is_from_shared_library)
                error("A R_X86_64_PC32 relocation cannot be used for symbol \"%s\" when making a shared object; recompile with -fPIC", symbol_name);

            return;

        case R_X86_64_TPOFF32:
        case R_X86_64_32:
        case R_X86_64_32S:
            return;

        case R_X86_64_GOTPCRELX: {
            if (link_dynamically) {
                add_got_or_plt_relocation(input_elf_file, relocation, RT_GOT);
                return;
            }

            // Relax instructions to not use the GOT

            uint32_t *output = (uint32_t *) input_data;

            uint8_t *pmod_rm = (uint8_t *) (input_data - 1);
            uint8_t *popcode = (uint8_t *) (input_data - 2);
            uint8_t *pprefix = (uint8_t *) (input_data - 3);
            uint8_t opcode = *popcode;

            if (offset > 1 && opcode == 0x8b) {
                if (output_is_shared) {
                    // TODO Convert movl foo@GOTPCREL@(%rip) to lea foo(%rip).
                    panic("Relaxing to relative-RIP load for R_X86_64_GOTPCRELX for not implemented for ET_DYN\n");
                }

                // Convert movl foo@GOTPCREL(%rip), %eax to mov $foo, %eax
                *popcode = 0xc7;
                *pmod_rm = 0xc0 | (*pmod_rm & 0x38) >> 3;

                return;
            }

            else if (offset > 1 && opcode == 0xff) {
                if (*pmod_rm == 0x15) {
                    // Convert callq foo(%rip) to addr32 callq foo
                    *popcode = 0x67;
                    *pmod_rm = 0xe8;
                }
                else
                    panic("Unhandled instruction rewrite for R_X86_64_GOTPCRELX: %#02x %#02x\n", opcode, *pmod_rm);

                return;
            }

            panic("Unhandled instruction rewrite for R_X86_64_GOTPCRELX: %#x\n", opcode);
        }

        case R_X86_64_GOTPCREL:
        case R_X86_64_REX_GOTPCRELX: {
            if (link_dynamically) {
                add_got_or_plt_relocation(input_elf_file, relocation, RT_GOT);
                return;
            }

            // Relax instructions to not use the GOT

            uint8_t *pprefix = (uint8_t *) (input_data - 3);
            uint8_t *popcode = (uint8_t *) (input_data - 2);
            uint8_t opcode = *popcode;

            if (offset > 2 && opcode == 0x8b) {
                if (output_is_shared) {
                    // Convert mov foo@GOTPCREL@(%rip) to lea foo(%rip).
                    // The RIP-relative value of foo will be used rather than a GOT slot.
                    *popcode = 0x8d; // lea

                    return;
                }

                // Convert movq foo@GOTPCREL(%rip), %rax to movq $foo, %rax
                convert_Gvqp_to_Evqp(input_data, 0xc7, 0);
                return;
            }

            else if (offset > 2 && (*pprefix == 0x48 || *pprefix == 0x4c) && opcode == 0x3b) {
                // Convert cmpq foo@GOTPCREL(%rip), %rax to cmpq $foo, %rax
                convert_Gvqp_to_Evqp(input_data, 0x81, 7);
                return;
            }

            else if (offset > 2 && (*pprefix == 0x48 || *pprefix == 0x4c) && opcode == 0x2b) {
                // Convert subq foo@GOTPCREL(%rip), %rax to subq $foo, %rax
                convert_Gvqp_to_Evqp(input_data, 0x81, 5);
                return;
            }

            add_got_or_plt_relocation(input_elf_file, relocation, RT_GOT);
            return;
        }

        case R_X86_64_PLT32:
            if (link_dynamically) {
                add_got_or_plt_relocation(input_elf_file, relocation, RT_PLT);
                return;
            }
            else
                panic("Unhandled R_X86_64_PLT32");

            break;

        default: {
            const char *relocation_name = type < RELOCATION_NAMES_COUNT ? RELOCATION_NAMES[type] : "UNKNOWN";
            panic("Unhandled relocation type %s", relocation_name);
        }
    }
}

// Given an input file, output file, input section and relocation, process the relocation by modifying the output.
// This function updates the values in the output ELF file. The code may already have been relaxed by
// scan_relocation().
// All ELF file details are abstracted away, so that function can be easily tested.
void apply_relocation(OutputElfFile *output_elf_file, InputElfFile *input_elf_file, InputSection *input_section, ElfRelocation *relocation) {
    Symbol *symbol = get_symbol_from_relocation(input_elf_file, relocation);

    int link_dynamically = link_symbol_dynamically(output_elf_file, symbol);

    int type = relocation->r_info & 0xffffffff;
    int symbol_index = relocation->r_info >> 32;

    // Get the ELF symbol
    ElfSymbol *elf_symbol = &input_elf_file->symbol_table[symbol_index];
    int elf_symbol_type = elf_symbol->st_info & 0xf;

    int dst_value = 0;
    char *symbol_name = NULL;

    // Get the output section
    OutputSection *rw_section = input_section->output_section;
    if (!rw_section) return; // The section is not included

    // Set several values depending on if the symbol points at a section or a symbol
    if (elf_symbol_type == STT_SECTION) {
        // Handle a relocation to a section symbol

        InputSection *symbol_section = (InputSection *) input_elf_file->section_list->elements[elf_symbol->st_shndx];
        symbol_name = symbol_section->name;

        OutputSection *symbol_output_section = symbol_section->output_section;
        if (!symbol_output_section) panic("Unexpected null section in output when applying relocations");

        dst_value = symbol_output_section->address + symbol_section->dst_offset + elf_symbol->st_value;
    }
    else {
        // Handle a relocation to a non-section symbol

        symbol_name = &input_elf_file->symbol_table_strings[elf_symbol->st_name];
        int version_index = 0;

        Symbol *symbol = lookup_symbol(input_elf_file, symbol_name, version_index);
        // Weak symbols are allowed to be undefined. The value defaults to zero.
        if (symbol) dst_value = symbol->dst_value;
    }

    if (DEBUG_RELOCATIONS) {
        const char *relocation_name = type < RELOCATION_NAMES_COUNT ? RELOCATION_NAMES[type] : "UNKNOWN";
        printf("  input section %s, rel=%s, offset %#08lx,  %s + %ld\n",
            input_section->name, relocation_name, relocation->r_offset, symbol_name, relocation->r_addend);
    }

    uint64_t output_offset = input_section->dst_offset + relocation->r_offset;
    void *output_pointer = rw_section->data + output_offset;
    int output_is_shared = output_elf_file->type == ET_DYN;

    // Determine the relocated value and write it to the output.
    // See table 4.9 of x86-64-ABI-1.0.pdf or
    // https://www.ucw.cz/~hubicka/papers/abi/node19.html

    uint64_t A = relocation->r_addend;
    uint64_t P = rw_section->address + output_offset;
    uint64_t S = dst_value;

    // When linking statically, a R_X86_64_PLT32 is treated like a R_X86_64_PC32,
    // unless the symbol is in the .iplt section for ifuncs.
    if (type == R_X86_64_PLT32 && symbol->extra != SE_IN_GOT_IPLT && !link_dynamically)
        type = R_X86_64_PC32;

    if (type == R_X86_64_GOTTPOFF) {
        // Convert a foo@GOTTPOFF(%rip) to foo@GOTPCREL(%rip)
        type = R_X86_64_REX_GOTPCRELX;
        S -= output_elf_file->tls_template_tls_offset;
    }

    if (DEBUG_RELOCATIONS) printf("    S=%#lx P=%#lx A=%#lx\n", S, P, A);

    switch (type) {
        case R_X86_64_64: {
            uint64_t value;

            // This  value is used in static executables,
            // This is also used in DWARF sections in dynamic executables.
            value = S + A;

            uint64_t *output = (uint64_t *) output_pointer;

            if (DEBUG_RELOCATIONS) printf("    value=%#lx\n", value);
            *output = value;
            break;
        }

        case R_X86_64_PC32: {
            uint32_t *output = (uint32_t *) output_pointer;
            uint64_t value = S + A - P;

            // Unusual case of accessing a TLS template variable directly.
            // This is mostly to make an unusual TLS test case work.
            if (symbol && symbol->type == STT_TLS) value += output_elf_file->tls_template_address;

            if (DEBUG_RELOCATIONS) printf("    value=%#lx\n", value);
            *output = value;
            break;
        }

        case R_X86_64_32:
        case R_X86_64_32S: {
            uint64_t value = S + A;
            uint32_t *output = (uint32_t *) output_pointer;
            if (DEBUG_RELOCATIONS) printf("    value=%#lx\n", value);
            *output = value;
            break;
        }
        case R_X86_64_GOTPCRELX: {
            // Relax instructions to not use the GOT

            uint32_t *output = (uint32_t *) output_pointer;
            uint8_t *popcode = (uint8_t *) (output_pointer - 2);
            uint8_t opcode = *popcode;

            if (output_offset > 1 && opcode == 0xc7) {
                uint64_t value = S; // Ignore the addend, this is an absolute address
                if (DEBUG_RELOCATIONS) printf("    value=%#lx\n", value);
                *output = value;
                break;
            }

            else if (output_offset > 1 && opcode == 0x67) {
                uint64_t value = S + A - P;
                if (DEBUG_RELOCATIONS) printf("    value=%#lx\n", value);
                *output = value;
                break;
            }

            panic("Unhandled relocation apply for R_X86_64_GOTPCRELX: %#x\n", opcode);
        }

        case R_X86_64_GOTPCREL:
        case R_X86_64_REX_GOTPCRELX: {
            uint8_t *popcode = (uint8_t *) (output_pointer - 2);
            uint8_t opcode = *popcode;

            uint64_t value = S;

            if (link_dynamically || (opcode != 0xc7 && opcode != 0x81)) {
                if (symbol->extra == SE_IN_GOT)
                    value = output_elf_file->got_virt_address + symbol->got_offset + A - P;
                else if (symbol->extra == SE_IN_GOT_IPLT)
                    value = output_elf_file->got_iplt_virt_address + symbol->got_iplt_offset + A - P;
                else
                    // The value has relaxed to RIP-relative addressing and has neither a GOT or PLT entry
                    value = S + A - P;
            }

            if (DEBUG_RELOCATIONS) printf("    value=%#lx\n", value);
            uint32_t *output = (uint32_t *) output_pointer;
            *output = value;
            break;
        }

        case R_X86_64_TPOFF32: {
            uint32_t *output = (uint32_t *) output_pointer;
            uint64_t value = S + A - output_elf_file->tls_template_tls_offset;

            if (DEBUG_RELOCATIONS) printf("    value=%#lx\n", value);
            *output = value;
            break;
        }

        case R_X86_64_PLT32: {
            if (symbol->extra == SE_IN_GOT_IPLT)
                S = output_elf_file->iplt_virt_address + symbol->iplt_offset + A - P;
            else if (symbol->extra == SE_IN_GOT_PLT)
                S = output_elf_file->plt_offset + symbol->plt_offset + A - P;
            else
                panic("Expected a value in the iplt, but no entry is present");

            if (DEBUG_RELOCATIONS) printf("    value=%#lx\n", S);
            uint32_t *output = (uint32_t *) output_pointer;
            *output = S;
            break;
        }

        default: {
            const char *relocation_name = type < RELOCATION_NAMES_COUNT ? RELOCATION_NAMES[type] : "UNKNOWN";
            panic("Unhandled relocation type %s for a relocation\n", relocation_name);
        }
    }
}

void process_relocations(OutputElfFile *output_elf_file, List *input_elf_files, int phase) {
    if (DEBUG_RELOCATIONS) printf("\nRelocations:\n");

    // Loop over all input files
    for (int i = 0; i < input_elf_files->length; i++) {
        InputElfFile *input_elf_file = input_elf_files->elements[i];

        // .so files don't get modified. The dynamic linker does that.
        if (input_elf_file->type == ET_DYN) continue;

        if (DEBUG_RELOCATIONS) printf("%s\n", input_elf_file->filename);

        // Loop over all relocation sections
        for (int j = 0; j < input_elf_file->section_list->length; j++) {
            InputSection *rela_input_section  = (InputSection *) input_elf_file->section_list->elements[j];
            if (rela_input_section->type != SHT_RELA) continue;

            int target_section_index = rela_input_section->info;
            InputSection *input_section  = (InputSection *) input_elf_file->section_list->elements[target_section_index];

            // Loop over all relocations
            ElfRelocation *relocations = load_section_uncached(input_elf_file, j);
            ElfRelocation *relocation = relocations;
            ElfRelocation *end = ((void *) relocations) + rela_input_section->size;

            while (relocation < end) {
                Symbol *symbol = get_symbol_from_relocation(input_elf_file, relocation);

                int link_dynamically = link_symbol_dynamically(output_elf_file, symbol);

                if (phase == RELOCATION_PHASE_SCAN)
                    scan_relocation(output_elf_file, input_elf_file, input_section, relocation);
                else if (phase == RELOCATION_PHASE_APPLY)
                    apply_relocation(output_elf_file, input_elf_file, input_section, relocation);
                relocation++;
            }

            free(relocations);
        }
    }
}
