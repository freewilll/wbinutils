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

// mod_rem encoding and decoding
#define MOD_RM_MODE_REGISTER_DIRECT_ADDRESSING 3

#define ENCODE_MOD_RM(mod, reg, rm) ((((mod) & 3) << 6) | (((reg) & 7) << 3) | ((rm) & 7))
#define DECODE_MOD_RM_MODE(mod_rm) ((mod_rm) >> 6)
#define DECODE_MOD_RM_REG(mod_rm) (((mod_rm) >> 3) & 7)
#define DECODE_MOD_RM_RM(mod_rm) ((mod_rm) & 7)

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

// Lookup a symbol in the symbol table for from an ELF symbol
// Looking up versioned symbols isn't implemented.
static Symbol *get_symbol_from_elf_symbol_index(InputElfFile *input_elf_file, int symbol_index) {
    ElfSymbol *elf_symbol = &input_elf_file->symbol_table[symbol_index];
    char *symbol_name = &input_elf_file->symbol_table_strings[elf_symbol->st_name];

    int elf_file_local_version_index = input_elf_file->symbol_table_version_indexes
        ? input_elf_file->symbol_table_version_indexes[symbol_index]
        : 0;

    int global_version_index = GLOBAL_SYMBOL_INDEX_NONE;

    if (elf_file_local_version_index >= 2)
        panic("Relocations to versioned symbols not implemented");

    return lookup_symbol(input_elf_file, symbol_name, global_version_index);
}

// Lookup a symbol in the symbol table for a relocation. If null, then the relocation refers to a section..
static Symbol *get_symbol_from_relocation(InputElfFile *input_elf_file, ElfRelocation *relocation) {
    int symbol_index = relocation->r_info >> 32;
    return get_symbol_from_elf_symbol_index(input_elf_file, symbol_index);
}

static int link_symbol_dynamically(OutputElfFile *output_elf_file, Symbol *symbol) {
    // Local symbols may not be bound dynamically
    if (symbol && symbol->binding == STB_LOCAL) return 0;

    int symbol_is_from_shared_library = 0;
    if (symbol && symbol->src_elf_file) symbol_is_from_shared_library = symbol->src_elf_file->type == ET_DYN;

    // Symbols in an executable cannot be preempted/interspersed, like possible in a shared library, so link them statically when possible.
    int link_dynamically = symbol_is_from_shared_library;
    if (!symbol_is_from_shared_library && output_elf_file->type == ET_DYN && !output_elf_file->is_executable) link_dynamically = 1;

    return link_dynamically;
}

// Determine if a symbol can have its instructions rewritten due to a GOTPCRELX relocation
// A symbol that can be preempted must be accessed indirectly
// GOTPCRELX relocations allow instruction relaxation when the symbol is non-preemptible and locally defined.
static int may_relax_symbol_for_GOTPCRELX(OutputElfFile *output_elf_file, Symbol *symbol) {
    if (!symbol) return 1; // Section symbols are always relaxable
    if (symbol->binding == STB_LOCAL) return 1; // Local symbols may always be relaxed
    if (output_elf_file->type == ET_EXEC) return 1; // Can always relax in static executable
    if (symbol->is_undefined) return 0; // Cannot relax undefined symbols
    if (symbol->binding == STB_WEAK) return 0; // Cannot relax weak symbols
    if (!output_elf_file->is_executable) return 0; // Is not preemptible

    return 1;
}

// A .got or .got.plt entry and a .dynsym entry
static void add_got_or_plt_relocation(InputElfFile *input_elf_file, ElfRelocation *relocation, int symbol_extra) {
    if (symbol_extra != SE_IN_GOT && symbol_extra != SE_IN_GOT_PLT) panic("Invalid symbol extra %d\n", symbol_extra);

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
    Symbol *symbol = get_symbol_from_elf_symbol_index(input_elf_file, symbol_index);
    if (!symbol) panic("Cannot process a relocation for an undefined symbol: %s\n", symbol->name);

    symbol->extra |= symbol_extra;
    symbol->needs_dynsym_entry = 1;
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

    Symbol *symbol = get_symbol_from_elf_symbol_index(input_elf_file, symbol_index);

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

static void add_R_X86_64_64_relocation(OutputElfFile *output_elf_file, InputElfFile *input_elf_file, InputSection *input_section, ElfRelocation *relocation) {
    // Ignore relocations for non-loadable sections, e.g. dwarf sections
    if (!(input_section->flags & SHF_ALLOC)) return;

    // A .rela.dyn entry is needed
    int symbol_index = relocation->r_info >> 32;
    ElfSymbol *elf_symbol = &input_elf_file->symbol_table[symbol_index];
    char *symbol_name = &input_elf_file->symbol_table_strings[elf_symbol->st_name];

    InputSection *relocation_input_section = NULL;

    Symbol *symbol = get_symbol_from_elf_symbol_index(input_elf_file, symbol_index);
    if (!symbol)
        panic("In add_R_X86_64_64_relocation: Expected symbol at index %d to be defined due to a relocation referencing it in %s",
            symbol_index,
            input_elf_file ? input_elf_file->filename : "(unknown filename)");

    symbol->needs_dynsym_entry = 1;

    RelativeRelaDynRelocation *rrdr = calloc(1, sizeof(RelativeRelaDynRelocation));
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
                if (symbol && (symbol_is_from_shared_library || !is_executable))
                    // The symbol is defined and either defined in another shared library,
                    // or or preemtible since we are building a shared library.
                    add_R_X86_64_64_relocation(output_elf_file, input_elf_file, input_section, relocation);
                else
                    // One of the following holds:
                    // The symbol is undefined, meaning it's local and not preemptible
                    // The symbol is defined and not from a shared library, so it's from an object file, so local.
                    // The symbol is defined and the output is an executable, so it's not preemtible
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

        case R_X86_64_GOTPCREL:
            add_got_or_plt_relocation(input_elf_file, relocation, SE_IN_GOT);
            return;

        case R_X86_64_GOTPCRELX: {
            // Relax instructions to not use the GOT, if possible.

            // GOTPCRELX relocations allow instruction relaxation when the symbol is non-preemptible and locally defined.
            int may_relax = may_relax_symbol_for_GOTPCRELX(output_elf_file, symbol);

            if (link_dynamically || !may_relax) {
                // A GOT entry must be added and no relaxation is possible
                add_got_or_plt_relocation(input_elf_file, relocation, SE_IN_GOT);
                return;
            }

            // The symbol has a known address

            char *debug_symbol_name = symbol ? symbol->name : "(none)";

            // Attempt to rewrite instructions, otherwise fall back to using the GOT

            uint8_t *pmod_rm = (uint8_t *) (input_data - 1);
            uint8_t *popcode = (uint8_t *) (input_data - 2);
            uint8_t *pprefix = (uint8_t *) (input_data - 3);
            uint8_t opcode = *popcode;

            // movl foo@GOTPCREL(%rip)
            if (offset > 1 && opcode == 0x8b) {
                if (output_is_shared) {
                    // TODO Convert movl foo@GOTPCREL(%rip), ... to lea foo(%rip), ....
                    panic("Relaxing to relative-RIP load for R_X86_64_GOTPCRELX not implemented for ET_DYN\n");
                }

                // Use the absolute value of the symbol in a static executable
                if (DEBUG_RELOCATION_RELAXATION)
                    printf("Relaxing mov %s@GOTPCREL(%%rip), reg to mov $%s, reg\n", debug_symbol_name,  debug_symbol_name);

                // Convert Gvqp to Evqp and set mode to direct addressing
                *popcode = 0xc7;
                *pmod_rm = 0xc0 | (*pmod_rm & 0x38) >> 3;

                return;
            }

            // callq foo(%rip)
            else if (offset > 1 && opcode == 0xff) {
                if (DEBUG_RELOCATION_RELAXATION)
                    printf("Relaxing callq %s(%%rip) to addr32 callq %s RIP-relative\n", debug_symbol_name,  debug_symbol_name);

                if (*pmod_rm == 0x15) { // reg=2, the opcode extension r/m=5 is [rip + disp32]. So mod/rm = reg << 3 + r/m = 0x15
                    *popcode = 0x67; // Replace the opcode with the address size override prefix 0x67
                    *pmod_rm = 0xe8; // Replace mod/rm with the new opcode 0xe8
                }
                else
                    panic("Unhandled instruction rewrite for R_X86_64_GOTPCRELX: %#02x %#02x\n", opcode, *pmod_rm);

                return;
            }

            panic("Unhandled instruction rewrite for R_X86_64_GOTPCRELX: %#x\n", opcode);
        }

        case R_X86_64_REX_GOTPCRELX: {
            // Relax instructions to not use the GOT, if possible.

            // GOTPCRELX relocations allow instruction relaxation when the symbol is non-preemptible and locally defined.
            int may_relax = may_relax_symbol_for_GOTPCRELX(output_elf_file, symbol);

            if (link_dynamically || !may_relax) {
                // A GOT entry must be added and no relaxation is possible
                add_got_or_plt_relocation(input_elf_file, relocation, SE_IN_GOT);
                return;
            }

            // The symbol has a known address

            char *debug_symbol_name = symbol ? symbol->name : "(none)";

            // Attempt to rewrite instructions, otherwise fall back to using the GOT

            uint8_t *pprefix = (uint8_t *) (input_data - 3);
            uint8_t *popcode = (uint8_t *) (input_data - 2);
            uint8_t opcode = *popcode;

            // See x86-64-ABI-1.0 B.2
            if (offset > 2 && opcode == 0x8b) {
                // Relax mov %s@GOTPCREL(%%rip)

                if (output_is_shared) {
                    // The RIP-relative value of foo will be used rather than a GOT slot.
                    if (DEBUG_RELOCATION_RELAXATION)
                        printf("Relaxing mov %s@GOTPCREL(%%rip) to lea %s(%%rip)\n", debug_symbol_name,  debug_symbol_name);

                    *popcode = 0x8d; // lea

                    return;
                }

                // Use the absolute value of the symbol in a static executable
                if (DEBUG_RELOCATION_RELAXATION)
                    printf("Relaxing movq %s@GOTPCREL(%%rip), raxreg to movq $%s, raxreg\n", debug_symbol_name,  debug_symbol_name);

                convert_Gvqp_to_Evqp(input_data, 0xc7, 0);
                return;
            }

            else if (!output_is_shared && offset > 2 && (*pprefix == 0x48 || *pprefix == 0x4c) && opcode == 0x3b) {
                if (DEBUG_RELOCATION_RELAXATION)
                    printf("Relaxing cmpq %s@GOTPCREL(%%rip), raxreg to cmpq $%s, raxreg\n", debug_symbol_name,  debug_symbol_name);

                convert_Gvqp_to_Evqp(input_data, 0x81, 7);
                return;
            }

            else if (!output_is_shared && offset > 2 && (*pprefix == 0x48 || *pprefix == 0x4c) && opcode == 0x2b) {
                if (DEBUG_RELOCATION_RELAXATION)
                    printf("Relaxing subq %s@GOTPCREL(%%rip), raxreg to subq $%s, raxreg\n", debug_symbol_name,  debug_symbol_name);

                convert_Gvqp_to_Evqp(input_data, 0x81, 5);
                return;
            }

            add_got_or_plt_relocation(input_elf_file, relocation, SE_IN_GOT);
            return;
        }

        case R_X86_64_PLT32:
            if (link_dynamically) {
                add_got_or_plt_relocation(input_elf_file, relocation, SE_IN_GOT_PLT);
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

        Symbol *symbol = get_symbol_from_elf_symbol_index(input_elf_file, symbol_index);

        // Weak symbols are allowed to be undefined. The value defaults to zero.
        if (symbol) dst_value = symbol->dst_value;
    }

    if (DEBUG_RELOCATIONS) {
        const char *relocation_name = type < RELOCATION_NAMES_COUNT ? RELOCATION_NAMES[type] : "UNKNOWN";
        char *symbol_name = symbol ? symbol->name : "(none)";
        printf("  input section %s, rel=%s, offset %#08lx,  %s + %ld symbol=%s\n",
            input_section->name, relocation_name, relocation->r_offset, symbol_name, relocation->r_addend, symbol_name);
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
    if (type == R_X86_64_PLT32 && symbol && !(symbol->extra & SE_IN_GOT_IPLT) && !link_dynamically)
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

            // This value is used in static executables,
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

        case R_X86_64_GOTPCREL: {
            uint64_t value;

            if (symbol->extra & SE_IN_GOT) {
                value = output_elf_file->got_virt_address + symbol->got_offset + A - P;
            }
            else if (symbol->extra & SE_IN_GOT_PLT) {
                panic("R_X86_64_GOTPCREL does not create SE_IN_GOT_PLT relocations");
            }
            else if (symbol->extra & SE_IN_GOT_IPLT)
                value = output_elf_file->got_iplt_virt_address + symbol->got_iplt_offset + A - P;
            else
                panic("Got a R_X86_64_GOTPCREL relocation without the symbol being in any of the GOT tables. symbol->extra=%d", symbol->extra);

            if (DEBUG_RELOCATIONS) printf("    value=%#lx\n", value);
            uint32_t *output = (uint32_t *) output_pointer;
            *output = value;
            break;
        }

        case R_X86_64_GOTPCRELX: {
            // Relax instructions to not use the GOT, where possible

            uint32_t *output = (uint32_t *) output_pointer;
            uint8_t *output1 = (uint8_t *) (output_pointer - 1);
            uint8_t *output2 = (uint8_t *) (output_pointer - 2);

            // If a symbol is the GOT, then write the PC-relative offset to the GOT entry
            if (symbol->extra & SE_IN_GOT) {
                uint64_t value = output_elf_file->got_virt_address + symbol->got_offset + A - P;
                uint32_t *output = (uint32_t *) output_pointer;
                *output = value;
                break;
            }

            // opcode 0xc7 is mov $foo, %eax, which was converted from foo@GOTPCREL(%rip), %eax in the scan phase
            // Make an absolute address
            else if (output_offset > 1 && (*output2 == 0xc7)) {
                uint64_t value = S; // Ignore the addend, this is an absolute address
                if (DEBUG_RELOCATIONS) printf("    value=%#lx\n", value);
                *output = value;
                break;
            }

            // addr32 callq foo RIP-relative encoded as 0x67 0xe8 which was converted from callq foo(%rip)
            // Make a RIP-relative address
            else if (output_offset > 1 && *output2 == 0x67 && *output1 == 0xe8) {
                uint64_t value = S + A - P;
                if (DEBUG_RELOCATIONS) printf("    value=%#lx\n", value);
                *output = value;
                break;
            }

            panic("Unhandled relocation apply for R_X86_64_GOTPCRELX: %#x %#x\n", *output2, *output1);
        }

        case R_X86_64_REX_GOTPCRELX: {
            // Instructions may have been relaxed. .got or .got.plt entries may also have been added.

            uint8_t *output1 = (uint8_t *) (output_pointer - 1);
            uint8_t *output2 = (uint8_t *) (output_pointer - 2);
            uint8_t *mod_rm = (uint8_t *) (output_pointer - 1);

            uint64_t value;

            // A relaxed binop usees an absolute address
            int use_absolute_address = *output2 == 0x81; // 0x81 is a binop $foo, %rax

            // Check if the instruction is a mov with an absolute address
            // 0xc7 is mov $foo, %eax
            if (*output2 == 0xc7 && DECODE_MOD_RM_MODE(*mod_rm) == MOD_RM_MODE_REGISTER_DIRECT_ADDRESSING)
                use_absolute_address = 1;

            // The instruction has been relaxed from a mov to a lea
            int use_pc_relative_address = *output2 == 0x8d;

            if (use_pc_relative_address)
                // The instruction has been relaxed from a mov to use an PC-relative address using a lea.
                value = S + A - P;
            else if (use_absolute_address)
                // The instruction has been relaxed to use an absolute value
                value = S;

            // The instruction has not been relaxed.
            else if (symbol->extra & SE_IN_GOT)
                value = output_elf_file->got_virt_address + symbol->got_offset + A - P;
            else if (symbol->extra & SE_IN_GOT_PLT)
                panic("R_X86_64_REX_GOTPCRELX does not create SE_IN_GOT_PLT relocations");
            else if (symbol->extra & SE_IN_GOT_IPLT)
                value = output_elf_file->got_iplt_virt_address + symbol->got_iplt_offset + A - P;
            else
                panic("Unable to determine the operation for a R_X86_64_REX_GOTPCRELX. Opcode=%#x", *output2);

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
            if (symbol && (symbol->extra & SE_IN_GOT_IPLT))
                S = output_elf_file->iplt_virt_address + symbol->iplt_offset + A - P;
            else if (symbol && (symbol->extra & SE_IN_GOT_PLT))
                S = output_elf_file->plt_offset + symbol->plt_offset + A - P;
            else
                panic("R_X86_64_PLT32 relocation symbol not in plt nor iplt");

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
