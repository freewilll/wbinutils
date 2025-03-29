#ifndef _UTILS_H
#define _UTILS_H

int string_ends_with(const char *string, const char *substring);

#define ALIGN_UP(offset, alignment) ((((offset) + alignment - 1) & ~(alignment - 1)))
#define PADDING_FOR_ALIGN_UP(offset, alignment) (ALIGN_UP((offset), (alignment)) - (offset))

#endif
