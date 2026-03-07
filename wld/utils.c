#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "error.h"
#include "list.h"
#include "input-elf.h"

#include "wld/libs.h"
#include "wld/utils.h"

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

// Try and find the library on the builtin library paths. Return NULL if not found
char *find_file(List *library_paths, const char *filename, const char *what) {
    int path_len = strlen(filename);

    int found = -1;
    char *test_path;

    // Search user paths
    for (int i = 0; i < library_paths->length; i++) {
        char *search_path = (char *) library_paths->elements[i];

        test_path = malloc(strlen(search_path) + path_len + 2);
        sprintf(test_path, "%s/%s", search_path, filename);

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
            sprintf(test_path, "%s/%s", BUILTIN_LIBRARY_PATHS[i], filename);

            if (!access(test_path, F_OK)) {
                found = i;
                break;
            }
        }
    }

    if (found == -1) return NULL;

    return test_path;
}

// Try and find the library on the builtin library paths. Fail if not present.
char *must_find_file(List *library_paths, const char *filename, const char *what) {
    char *path = find_file(library_paths, filename, what);
    if (!path) error("Cannot find %s %s", what, path);
    return path;
}

FileType identify_library_file(const char *filename) {
    FILE *file = fopen(filename, "r");

    if (!file) {
        perror(filename);
        exit(1);
    }

    if (file_is_archive_file(file, filename))
        return FT_ARCHIVE;
    else if (file_is_shared_library_file(filename))
        return FT_SHARED_LIBRARY;
    else if (is_gnu_linker_script_file(filename))
        return FT_LINKER_SCRIPT;
    else
        return FT_UNKNOWN;

}

// Match a string to a pattern. The pattern may have * and ?. [] isn't implemented
int match_pattern(const char *string, const char *pattern) {
    const char *s = string;
    const char *p = pattern;
    const char *star = NULL;  // The string at the point a star was found
    const char *ss = NULL;    // The string when a star was first encountered

    while (*s) {
        if (*p == '?' || *p == *s) {
            s++;
            p++;
        } else if (*p == '*') {
            star = p++;
            ss = s;
        } else if (star) {
            // No match, and we're processing a star
            p = star + 1;   // Backtrack: reset p
            s = ++ss;       // Move s to one character further forward
        } else {
            // No match, and not doing a *
            return 0;
        }
    }

    while (*p == '*') p++;  // Skip trailing *

    return *p == '\0';
}

// Match a path to a pattern. A potential path is stripped off first.
int match_path_pattern(const char *path, const char *pattern) {
    char *p = strrchr(path, '/');
    if (p)
        return match_pattern(p + 1, pattern);
    else
        return match_pattern(path, pattern);
}

int is_c_identifier(const char *name) {
    if (!name[0]) return 0;

    // It must start with A-Z, a-z or _
    if (!((name[0] >= 'A' && name[0] <= 'Z') || (name[0] >= 'a' && name[0] <= 'z') || name[0] == '_')) return 0;

    const char *p = name;
    // It must containt only  A-Z, a-z, 0-9 or _
    while (*p) {
        if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_')) return 0;
        *p++;
    }

    return 1;
}