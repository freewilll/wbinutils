#include <stdlib.h>
#include <string.h>

#include "output-elf.h"

#include "was/list.h"
#include "was/strmap.h"
#include "was/utils.h"
#include "was/was.h"

OutputElfFile *output_elf_file;

void init_elf_file(const char *output_filename) {
    output_elf_file = new_output_elf_file(output_filename, ET_REL);
}

// Final stage of the assembly
void finish_elf(char *filename) {
    // Make all ELF section headers
    make_output_section_headers(output_elf_file);

    // Layout the sections & allocate memory for the output
    layout_output_elf_sections(output_elf_file);

    // Make the ELF headers and section headers
    make_elf_headers(output_elf_file);

    // Write the ELF file
    write_elf_file(output_elf_file);
}
