#include "list.h"

#include "wld/lexer.h"
#include "wld/parser.h"
#include "wld/utils.h"
#include "wld/script.h"

List *linker_script;

void parse_linker_scripts(List *library_paths, List *linker_scripts) {
    linker_script = new_list(1);

    for (int i = 0; i < linker_scripts->length; i++) {
        char *filename = linker_scripts->elements[i];
        char *path = find_file(library_paths, filename, "linker script");
        init_lexer(path);
        parse();
    }
}