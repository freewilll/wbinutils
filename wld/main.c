#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "list.h"

#include "wld/wld.h"
#include "wld/script.h"

int main(int argc, char **argv) {
    int exit_code = 0;
    int help = 0;
    int verbose = 0;
    int is_static = 0;
    int is_shared = 0;
    int is_pie = 0; // Position independent executable
    List *input_files = new_list(32);
    char *output_filename = NULL;
    List *library_paths = new_list(32);
    List *linker_scripts = new_list(32);
    char *dynamic_linker = NULL;
    char *soname = NULL;
    List *rpaths = new_list(1);

    argc--;
    argv++;
    while (argc > 0) {
        if (*argv[0] == '-') {
                 if (argc > 0 && !strcmp(argv[0], "-h"   )) { help = 1;    argc--; argv++; }
            else if (!strcmp(argv[0], "-v")) {
                verbose = 1;
                argc--;
                argv++;
            }
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
                input_file->is_library_name = 1;
                append_to_list(input_files, input_file);
                argc -= 2;
                argv += 2;
            }
            // -lx
            else if (strlen(argv[0]) > 2 && argv[0][0] == '-' && argv[0][1] == 'l' ) {
                InputFile *input_file = malloc(sizeof(InputFile));
                input_file->filename = &argv[0][2];
                input_file->is_library_name = 1;
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
                is_static = 1;
                argc--;
                argv++;
            }
            else if (!strcmp(argv[0], "-shared")) {
                is_shared = 1;
                argc--;
                argv++;
            }
            else if (!strcmp(argv[0], "-pie")) {
                is_pie = 1;
                argc--;
                argv++;
            }
            // -dynamic-linker x
            else if (argc > 1 && !strcmp(argv[0], "-dynamic-linker")) {
                dynamic_linker = argv[1];
                argc -= 2;
                argv += 2;
            }
            // -soname x
            else if (argc > 1 && !strcmp(argv[0], "-soname")) {
                soname = argv[1];
                argc -= 2;
                argv += 2;
            }
            // -rpath x
            else if (argc > 1 && !strcmp(argv[0], "-rpath")) {
                append_to_list(rpaths, argv[1]);
                argc -= 2;
                argv += 2;
            }
            else {
                error("Unknown parameter %s\n", argv[0]);
            }
        }
        else {
            InputFile *input_file = malloc(sizeof(InputFile));
            input_file->filename = argv[0];
            input_file->is_library_name = 0;
            append_to_list(input_files, input_file);
            argc--;
            argv++;
        }
    }

    if (help) {
        printf("Usage: wld [-] [-o OUTPUT-FILE] INPUT-FILE...\n\n");
        printf("Flags\n");
        printf("-h               Help\n");
        printf("-o               Output filename\n");
        printf("-T               Use linker script\n");
        printf("-v               Output debugging information\n");
        printf("-static          Link static executable\n");
        printf("-shared          Link shared library\n");
        printf("-dynamic-linker  Set the name of the dynamic linker\n");
        printf("-pie             Mostly ignored, is present for compatibility with gcc\n");
        printf("-soname          The name set in DT_SONAME in a shared library\n");
        printf("-rpath           Add a path to the runpath search list\n");
        exit(1);
    }

    int output_type = 0;
    if (is_shared) output_type |= OUTPUT_TYPE_FLAG_SHARED;
    if (is_static) output_type |= OUTPUT_TYPE_FLAG_STATIC;
    if (!is_shared) output_type |=OUTPUT_TYPE_FLAG_EXECUTABLE;
    if (!is_static && !is_shared) output_type |= OUTPUT_TYPE_FLAG_SHARED; // For a  dynamic executable

    // is_pie is the default for dynamic executables, so the flag is ignored.
    // However, the flag is invalid when making a shared library or a static executable.
    if (is_pie && (output_type & !((OUTPUT_TYPE_FLAG_SHARED | OUTPUT_TYPE_FLAG_EXECUTABLE)))) {
        printf("The -pie flag can only be used with dynamic executables\n");
        exit(1);
    }

    if (verbose) {
        printf("Wld linker\n");

        // Print linker script
        char *output_filename = "dummy";
        OutputElfFile *output_elf_file = init_output_elf_file(output_filename, output_type);
        char *linker_script = default_linker_script(output_elf_file);
        printf("Linker script:\n");
        printf("--------------\n");
        puts(linker_script);

        exit(0);
    }

    if (!output_filename) output_filename = "a.out";

    if (!input_files->length) {
        error("Missing input filename");
    }

    run(library_paths, linker_scripts, input_files, output_filename, output_type, dynamic_linker, soname, rpaths);

    exit(exit_code);
}
