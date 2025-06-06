#include "list.h"

#include "rw-elf.h"

#include "wld/lexer.h"
#include "wld/parser.h"
#include "wld/utils.h"
#include "wld/script.h"

void parse_linker_scripts(RwElfFile *output_elf_file, List *library_paths, List *linker_scripts) {
    if (linker_scripts->length == 0) {
        init_lexer_from_string(DEFAULT_LINKER_SCRIPT);
        output_elf_file->linker_script = parse();
        return;
    }

    for (int i = 0; i < linker_scripts->length; i++) {
        char *filename = linker_scripts->elements[i];
        char *path = find_file(library_paths, filename, "linker script");
        init_lexer(path);
        output_elf_file->linker_script = parse();
    }
}
