#include "list.h"

#include "output-elf.h"

#include "wld/lexer.h"
#include "wld/parser.h"
#include "wld/utils.h"
#include "wld/script.h"

char *default_linker_script(OutputElfFile *output_elf_file) {
    if (output_elf_file->type == ET_EXEC)
        return DEFAULT_LINKER_SCRIPT_STATIC;
    else
        return DEFAULT_LINKER_SCRIPT_SHARED;
}

void parse_linker_scripts(OutputElfFile *output_elf_file, List *library_paths, List *linker_scripts) {
    if (linker_scripts->length == 0) {
        init_lexer_from_string(default_linker_script(output_elf_file));
        output_elf_file->linker_script = parse();
        return;
    }

    for (int i = 0; i < linker_scripts->length; i++) {
        char *filename = linker_scripts->elements[i];
        char *path = must_find_file(library_paths, filename, "linker script");
        init_lexer(path);
        output_elf_file->linker_script = parse();
    }
}
