#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include  "error.h"

#include "was/was.h"
#include "was/utils.h"

int main(int argc, char **argv) {
    int exit_code = 0;
    int help = 0;
    int verbose = 0;
    char *input_filename = NULL;
    char *output_filename = NULL;

    argc--;
    argv++;
    while (argc > 0) {
        if (*argv[0] == '-') {
                 if (argc > 0 && !strcmp(argv[0], "-h"   )) { help = 1;    argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-v"   )) { verbose = 1; argc--; argv++; }
            else if (argc > 0 && !strcmp(argv[0], "-64"  )) {              argc--; argv++; }
            else if (argc > 1 && !memcmp(argv[0], "-o", 2)) {
                output_filename = argv[1];
                argc -= 2;
                argv += 2;
            }
            else {
                error("Unknown parameter %s", argv[0]);
            }
        }
        else {
            if (input_filename) {
                error("Multiple input filenames not supported");
            }
            input_filename = argv[0];
            argc--;
            argv++;
        }
    }

    if (help) {
        printf("Usage: was [-h -v] [-o OUTPUT-FILE] INPUT-FILE...\n\n");
        printf("Flags\n");
        printf("-h      Help\n");
        printf("-v      Display the programs invoked by the compiler\n");
        printf("-o      Output filename\n");
        printf("-64     Select x86-64 architecture (for compatibility with gnu as)\n");
        exit(1);
    }

    if (verbose) printf("Was assembler\n");

    if (!output_filename) output_filename = "a.out";

    if (!input_filename) {
        error("Missing input filename");
    }

    assemble(input_filename, output_filename);

    exit(exit_code);
}
