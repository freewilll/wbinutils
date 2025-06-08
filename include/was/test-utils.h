#include <stdarg.h>

#include "output-elf.h"
#include "lexer.h"
#include "parser.h"
#include "relocations.h"
#include "symbols.h"

#define START 0 // Dummy value
#define END -1
#define ENDL -1L // End for a string

void init_tests(void);
void test_full_assembly(char *summary, char *input, ...);
int dump_section(OutputSection *section);
void vassert_section_data(OutputSection* section, va_list ap);
void assert_section_data(OutputSection* section, ...);
void assert_relocations(char *section_name, ...);
void dump_symbols(void);
void assert_symbols(int first, ...);
void assert_section(char *name, int type, int flags);
int get_symbol_symtab_index(char *name);
void assert_dwarf_dirs(char *first, ...);
void assert_dwarf_files(int first, ...);
void assert_dwarf_line_program(int dummy, ...);
