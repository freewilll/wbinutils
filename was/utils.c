#include <string.h>

// Returns 1 if string ends with substring
int string_ends_with(const char *string, const char *substring) {
    int string_len = strlen(string);
    int substring_len = strlen(substring);

    if (string_len < substring_len) return 0;

    return !memcmp(string + string_len - substring_len, substring, substring_len);
}
