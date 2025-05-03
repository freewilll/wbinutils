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
    List *input_files = new_list(32);
    char *output_filename = NULL;
    List *library_paths = new_list(32);
    List *linker_scripts = new_list(32);

    argc--;
    argv++;
    while (argc > 0) {
        if (*argv[0] == '-') {
                 if (argc > 0 && !strcmp(argv[0], "-h"   )) { help = 1;    argc--; argv++; }
            else if (argc > 1 && !strcmp(argv[0], "-o")) {
                output_filename = argv[1];
                argc -= 2;
                argv += 2;
            }
            // -L x
            else if (argc > 1 && !strcmp(argv[0], "-L")) {
                InputFile *input_file = malloc(sizeof(InputFile));
                append_to_list(library_paths, argv[1]);
                argc -= 2;
                argv += 2;
            }
            // -Lx
            else if (strlen(argv[0]) > 2 && argv[0][0] == '-' && argv[0][1] == 'L' ) {
                InputFile *input_file = malloc(sizeof(InputFile));
                append_to_list(library_paths, &argv[0][2]);
                argc--;
                argv++;
            }
            // -l x
            else if (argc > 1 && !strcmp(argv[0], "-l")) {
                InputFile *input_file = malloc(sizeof(InputFile));
                input_file->filename = argv[1];
                input_file->is_library = 1;
                append_to_list(input_files, input_file);
                argc -= 2;
                argv += 2;
            }
            // -lx
            else if (strlen(argv[0]) > 2 && argv[0][0] == '-' && argv[0][1] == 'l' ) {
                InputFile *input_file = malloc(sizeof(InputFile));
                input_file->filename = &argv[0][2];
                input_file->is_library = 1;
                append_to_list(input_files, input_file);
                argc--;
                argv++;
            }
            // -T x
            else if (argc > 1 && !strcmp(argv[0], "-T")) {
                append_to_list(linker_scripts, argv[1]);
                argc -= 2;
                argv += 2;
            }
            else if (!strcmp(argv[0], "-static")) {
                // Do nothing
                argc--;
                argv++;
            }
            else {
                error("Unknown parameter %s\n", argv[0]);
            }
        }
        else {
            InputFile *input_file = malloc(sizeof(InputFile));
            input_file->filename = argv[0];
            input_file->is_library = 0;
            append_to_list(input_files, input_file);
            argc--;
            argv++;
        }
    }

    if (help) {
        printf("Usage: wld [-] [-o OUTPUT-FILE] INPUT-FILE...\n\n");
        printf("Flags\n");
        printf("-h       Help\n");
        printf("-o       Output filename\n");
        printf("-static  Link static executable\n");
        exit(1);
    }

    if (verbose) {
        printf("Wld linker\n");
        exit(1);
    }

    if (!output_filename) output_filename = "a.out";

    if (!input_files->length) {
        error("Missing input filename");
    }

    run(library_paths, linker_scripts, input_files, output_filename);

    exit(exit_code);
}
