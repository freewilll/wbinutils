#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "error.h"
#include "list.h"

const char *BUILTIN_LIBRARY_PATHS[] = {
    "/usr/local/lib/x86_64-linux-gnu",
    "/lib/x86_64-linux-gnu",
    "/usr/lib/x86_64-linux-gnu",
    "/usr/lib/x86_64-linux-gnu64",
    "/usr/local/lib64",
    "/lib64",
    "/usr/lib64",
    "/usr/local/lib",
    "/lib",
    "/usr/lib",
    "/usr/x86_64-linux-gnu/lib64",
    "/usr/x86_64-linux-gnu/lib",
};

// Try and find the library on the builtin library paths
char *find_file(List *library_paths, const char *path, const char *what) {
    int path_len = strlen(path);

    int found = -1;
    char *test_path;

    // Search user paths
    for (int i = 0; i < library_paths->length; i++) {
        char *search_path = (char *) library_paths->elements[i];

        test_path = malloc(strlen(search_path) + path_len + 2);
        sprintf(test_path, "%s/%s", search_path, path);

        if (!access(test_path, F_OK)) {
            found = i;
            break;
        }
    }

    if (found == -1) {
        // Search system paths
        int system_library_count = sizeof(BUILTIN_LIBRARY_PATHS) / sizeof(BUILTIN_LIBRARY_PATHS[0]);
        for (int i = 0; i < system_library_count; i++) {
            test_path = malloc(strlen(BUILTIN_LIBRARY_PATHS[i]) + path_len + 2);
            sprintf(test_path, "%s/%s", BUILTIN_LIBRARY_PATHS[i], path);

            if (!access(test_path, F_OK)) {
                found = i;
                break;
            }
        }
    }

    if (found == -1) error("Cannot find %s %s", what, path);

    return test_path;
}
