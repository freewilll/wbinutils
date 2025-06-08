#include <stdio.h>

#include "was/branches.h"
#include "was/dwarf.h"
#include "was/elf.h"
#include "was/lexer.h"
#include "was/opcodes.h"
#include "was/parser.h"
#include "was/relocations.h"
#include "was/was.h"

void emit_code(void) {
    for (int i = 0; i < output_elf_file->sections_list->length; i++) {
        OutputSection *section = output_elf_file->sections_list->elements[i];
        if (section->chunks) layout_section(section);
    }

    for (int i = 0; i < output_elf_file->sections_list->length; i++) {
        OutputSection *section = output_elf_file->sections_list->elements[i];
        if (section->chunks) emit_section_code(section);
    }
}

void assemble(char *input_filename, char *output_filename) {
    init_elf_file(output_filename);
    init_lexer(input_filename);
    init_symbols();
    init_default_sections();
    init_relocations();
    init_opcodes();
    init_parser();
    init_dwarf();
    parse();
    emit_code();
    make_dwarf_debug_line_section();
    make_section_indexes(output_elf_file);
    make_symbols_section();
    make_rela_sections();
    finish_elf(output_filename);
    free_lexer();
}
