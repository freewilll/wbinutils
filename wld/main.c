#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "list.h"
#include "wld/wld.h"

int main(int argc, char **argv) {
    int exit_code = 0;
    int help = 0;
    int verbose = 0;
    List *input_filenames = new_list(32);
    char *output_filename = NULL;

    argc--;
    argv++;
    while (argc > 0) {
        if (*argv[0] == '-') {
                 if (argc > 0 && !strcmp(argv[0], "-h"   )) { help = 1;    argc--; argv++; }
            else if (argc > 1 && !memcmp(argv[0], "-o", 2)) {
                output_filename = argv[1];
                argc -= 2;
                argv += 2;
            }
            else {
                error("Unknown parameter %s\n");
            }
        }
        else {
            append_to_list(input_filenames, argv[0]);
            argc--;
            argv++;
        }
    }

    if (help) {
        printf("Usage: wld [-] [-o OUTPUT-FILE] INPUT-FILE...\n\n");
        printf("Flags\n");
        printf("-h      Help\n");
        printf("-o      Output filename\n");
        exit(1);
    }

    if (verbose) {
        printf("Wld linker\n");
        exit(1);
    }

    if (!output_filename) output_filename = "a.out";

    if (!input_filenames->length) {
        error("Missing input filename");
    }

    run(input_filenames, output_filename);

    exit(exit_code);
}
