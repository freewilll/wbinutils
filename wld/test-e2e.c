#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "list.h"
#include "error.h"

#include "wld/symbols.h"
#include "wld/wld.h"

#define END -1

static void fail_sections(RwElfFile *elf_file, List *expected_sections) {
    printf("Got sections:\n");
    dump_sections(elf_file);

    // Brutally clobber the ELF file's sections. We're bailing anyways so who cares.
    printf("\nExpected sections:\n");
    elf_file->sections_list = expected_sections;
    dump_sections(elf_file);
    exit(1);
}

static void assert_sections(RwElfFile *elf_file, ...) {
    List *expected_sections = new_list(32);

    va_list ap;
    va_start(ap, elf_file);

    while (1) {
        char *name = va_arg(ap, char *);
        if (!name) break;

        int type = va_arg(ap, int);
        int address = va_arg(ap, int);
        int offset = va_arg(ap, int);
        int size = va_arg(ap, int);
        int flags = va_arg(ap, int);
        int align = va_arg(ap, int);

        RwSection *section = calloc(1, sizeof(RwSection));
        section->name = strdup(name);
        section->type = type;
        section->address = address;
        section->offset = offset;
        section->size = size;
        section->flags = flags;
        section->align = align;

        append_to_list(expected_sections, section);
    }

    // The executable has 4 extra sections: null, symtab, strtab and shstrdtab
    if (elf_file->sections_list->length - 4 != expected_sections->length) {
        printf("Sections lengths mismatch: expected %d, got %d\n", elf_file->sections_list->length - 4, expected_sections->length);
        fail_sections(elf_file, expected_sections);
    }

    for (int i = 0; i < expected_sections->length; i++) {
        RwSection *got = elf_file->sections_list->elements[i + 1];
        RwSection *expected = expected_sections->elements[i];

        if (
            strcmp(got->name, expected->name) ||
            got->type != expected->type ||
            got->address != expected->address ||
            got->offset != expected->offset ||
            got->size != expected->size ||
            got->flags != expected->flags ||
            got->align != expected->align) {

            printf("Sections mismatch at position %d in expected sections list\n", i);
            fail_sections(elf_file, expected_sections);
        }
    }
}

static void fail_program_segments(RwElfFile *elf_file, List *expected_program_segments) {
    printf("Got program_segments:\n");
    dump_program_segments(elf_file);

    // Brutally clobber the ELF file's program_segments. We're bailing anyways so who cares.
    printf("\nExpected program_segments:\n");
    elf_file->program_segments_list = expected_program_segments;
    dump_program_segments(elf_file);
    exit(1);
}

static void assert_program_segments(RwElfFile *elf_file, ...) {
    List *expected_program_segments = new_list(32);

    va_list ap;
    va_start(ap, elf_file);

    while (1) {
        int type = va_arg(ap, int);
        if (type == END) break;

        int offset = va_arg(ap, int);
        int vaddr = va_arg(ap, int);
        int filesz = va_arg(ap, int);
        int memsz = va_arg(ap, int);
        int flags = va_arg(ap, int);
        int align = va_arg(ap, int);

        ElfProgramSegmentHeader *program_segment = calloc(1, sizeof(ElfProgramSegmentHeader));
        program_segment->p_type = type;
        program_segment->p_offset = offset;
        program_segment->p_vaddr = vaddr;
        program_segment->p_filesz = filesz;
        program_segment->p_memsz = memsz;
        program_segment->p_flags = flags;
        program_segment->p_align = align;

        append_to_list(expected_program_segments, program_segment);
    }

    if (elf_file->program_segments_list->length != expected_program_segments->length) {
        printf("Sections lengths mismatch: expected %d, got %d\n", elf_file->program_segments_list->length , expected_program_segments->length);
        fail_program_segments(elf_file, expected_program_segments);
    }

    for (int i = 0; i < expected_program_segments->length; i++) {
        ElfProgramSegmentHeader *got = elf_file->program_segments_list->elements[i];
        ElfProgramSegmentHeader *expected = expected_program_segments->elements[i];

        if (
            got->p_type != expected->p_type ||
            got->p_offset != expected->p_offset ||
            got->p_vaddr != expected->p_vaddr ||
            got->p_filesz != expected->p_filesz ||
            got->p_memsz != expected->p_memsz ||
            got->p_flags != expected->p_flags ||
            got->p_align != expected->p_align) {

            printf("Program segments mismatch at position %d in expected program segments list\n", i);
            fail_program_segments(elf_file, expected_program_segments);
        }
    }
}

static void assert_symbols(RwElfFile *elf_file, ...) {
    RwSection *section = elf_file->section_symtab;

    va_list ap;
    va_start(ap, elf_file);

    int pos = 1 * sizeof(ElfSymbol); // Skip null symbol

    while (1) {
        int expected_value = va_arg(ap, int);

        if (expected_value == END) {
            if (pos != section->size) {
                dump_rw_symbols(elf_file);
                panic("Unexpected data at position %d", (pos - 1) / sizeof(ElfSymbol));
            }

            return; // Success
        }

        int expected_size = va_arg(ap, int);
        int expected_type = va_arg(ap, int);
        int expected_binding = va_arg(ap, int);
        int expected_visibility = va_arg(ap, int);
        char *expected_section_name = va_arg(ap, char *);
        char *expected_name = va_arg(ap, char *);

        int expected_section_index;
        if (!strcmp(expected_section_name, "ABS")) {
            expected_section_index = SHN_ABS;
        }
        else if (!strcmp(expected_section_name, "COM")) {
            expected_section_index = SHN_COMMON;
        }
        else {
            RwSection *expected_section = get_rw_section(elf_file, expected_section_name);
            if (!expected_section) {
                dump_rw_symbols(elf_file);
                panic("Section %s not found", expected_section_name);
            }
            expected_section_index = expected_section->index;
        }

        ElfSymbol *symbol = NULL;
        while (pos < section->size) {
            symbol = (ElfSymbol *) &section->data[pos];
            int type = symbol->st_info & 0xf;
            if (type != STT_SECTION && type != STT_FILE) break;

            pos += sizeof(ElfSymbol);
        }

        if (pos == section->size) {
            debug_summarize_symbols();
            panic("Expected extra data");
        }

        int got_value = symbol->st_value;
        int got_size = symbol->st_size;
        int got_type = symbol->st_info & 0xf;
        char got_binding = (symbol->st_info >> 4) & 0xf;
        char got_visibility = symbol->st_other & 3;
        unsigned short got_index = symbol->st_shndx;
        char *got_name = symbol->st_name ? &elf_file->section_strtab->data[symbol->st_name] : 0;

        int name_matches = ((!got_name && !expected_name) || (expected_name && !strcmp(expected_name, got_name)));

        if (
                expected_value != got_value ||
                expected_size != got_size ||
                expected_type != got_type ||
                expected_binding != got_binding ||
                expected_visibility != got_visibility ||
                expected_section_index != got_index ||
                !name_matches) {
            debug_summarize_symbols();
            panic("Symbols mismatch at position %d: expected %#lx, %#lx, %d, %d, %d, %s, got %#lx, %#lx, %d, %d, %d, %d, %s",
                (pos - 1) / sizeof(ElfSymbol),
                expected_value,
                expected_size,
                expected_type,
                expected_binding,
                expected_section_index,
                expected_name ? expected_name : "null",
                got_value,
                got_size,
                got_type,
                got_binding,
                got_visibility,
                got_index,
                got_name ? got_name : "null"
            );
        }

        pos += sizeof(ElfSymbol);
    }
}

static char *run_was(char *assembly) {
    // Write out assembly to a temp file
    char source_path[] = "/tmp/asm_XXXXXX";
    int fd = mkstemp(source_path);
    if (fd == -1) { perror("mkstemp"); exit(1); }

    FILE *source_file = fdopen(fd, "w");
    if (!source_file) { perror("fdopen"); exit(1); }
    fprintf(source_file, "%s\n", assembly);
    fclose(source_file);

    char object_path[] = "/tmp/asm_XXXXXX";
    fd = mkstemp(object_path);
    if (fd == -1) { perror("mkstemp"); exit(1); }

    // Assemble using ../bin/was
    char command[512];
    snprintf(command, sizeof(command), "../bin/was -o %s %s", object_path, source_path);
    int result = system(command);
    if (result) {
        printf("Was failed with exit code %d\n", result >> 8);
        exit(1);
    }

    return strdup(object_path);
}

static RwElfFile *run_wld(List *input_filenames, char **poutput_path, int run_executable, char *test_name) {
    // Make list of InputFile from the input filenames
    List *input_files = new_list(input_filenames->length);
    for (int i = 0; i < input_filenames->length; i++) {
        char *filename = input_filenames->elements[i];
        InputFile *input_file = malloc(sizeof(InputFile));
        input_file->filename = filename;
        input_file->is_library = 0;
        append_to_list(input_files, input_file);
    }

    // Make the output filename path
    char output_path[] = "/tmp/executable_XXXXXX";
    int fd = mkstemp(output_path);
    if (fd == -1) { perror("mkstemp"); exit(1); }
    *poutput_path = strdup(output_path);
    close(fd);

    List *library_paths = new_list(0);
    List *linker_scripts = new_list(0);

    RwElfFile *elf_file = run(library_paths, linker_scripts, input_files, output_path);

    if (run_executable) {
        // Run the executable and assert an exit code of zero

        int result = system(output_path);
        if (result) {
            printf("%s: Running %s failed with exit code %d\n", test_name, output_path, result >> 8);
            exit(1);
        }
    }

    return elf_file;
}

// A simple test with two sections
static void test_sanity() {
    char *object_path = run_was(
        ".globl _start;"
        ".text;"
        "_start:;"
        "    movl $1, %eax;"
        "    lea code(%rip), %rdi;"
        "    mov (%rdi), %ebx;"
        "    int $0x80;"
        ".section .data;"
        "    code: .long 0"
    );

    List *input_paths = new_list(1);
    append_to_list(input_paths, object_path);

    char *output_path;
    RwElfFile *elf_file = run_wld(input_paths, &output_path, 1, "sanity");

    assert_sections(elf_file,
        // Name           Type            Address   Offset  Size   Flags                      Align
        ".text",          SHT_PROGBITS,   0x401000, 0x1000, 0x10,  SHF_ALLOC | SHF_EXECINSTR, 16,
        ".data",          SHT_PROGBITS,   0x402000, 0x2000, 0x04,  SHF_ALLOC | SHF_WRITE,     4,
        NULL
    );

    assert_program_segments(elf_file,
        // Type           Offset   VirtAddr   FileSiz  MemSiz  Flags         Align
        PT_LOAD,          0x1000,  0x401000,  0x10,    0x10,   PF_R | PF_X,  0x1000,
        PT_LOAD,          0x2000,  0x402000,  0x04,    0x04,   PF_R | PF_W,  0x1000,
        END
    );

    assert_symbols(elf_file,
    //  Value      Size   Type        Binding     Visibility   Section   Name
        0,         0,     STT_NOTYPE, STB_GLOBAL, STV_HIDDEN,   "ABS",   "_GLOBAL_OFFSET_TABLE_",
        0x401000,  0,     STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, ".text", "_start",
        END);
}

// Test an orphan section with different flags after a data section.
// They must end up in their own pages, otherwise the kernel will drop
// flags in a common page mapping and cause mayham.
void test_segments_are_page_aligned() {
    char *object_path = run_was(
        ".globl _start;"
        ".text;"
        "_start:;"
        "    movl $1, %eax;"
        "    lea code(%rip), %rdi;"
        "    mov (%rdi), %ebx;"
        "    int $0x80;"
        ".section .data;"
        "    code: .long 0;"
        // A new section with different flags gets its own segment, which must be page aligned
        ".section .orphan, \"awx\", @progbits;"
        "    .long 0;"
    );

    List *input_paths = new_list(1);
    append_to_list(input_paths, object_path);

    char *output_path;
    RwElfFile *elf_file = run_wld(input_paths, &output_path, 1, "sanity");

    assert_sections(elf_file,
        // Name           Type            Address   Offset  Size   Flags                                  Align
        ".text",          SHT_PROGBITS,   0x401000, 0x1000, 0x10,  SHF_ALLOC | SHF_EXECINSTR,             16,
        ".data",          SHT_PROGBITS,   0x402000, 0x2000, 0x04,  SHF_ALLOC | SHF_WRITE,                 4,
        ".orphan",        SHT_PROGBITS,   0x403000, 0x3000, 0x04,  SHF_ALLOC | SHF_EXECINSTR | SHF_WRITE, 1,
        NULL
    );

    assert_program_segments(elf_file,
        // Type           Offset   VirtAddr   FileSiz  MemSiz  Flags               Align
        PT_LOAD,          0x1000,  0x401000,  0x10,    0x10,   PF_R | PF_X,        0x1000,
        PT_LOAD,          0x2000,  0x402000,  0x04,    0x04,   PF_R | PF_W,        0x1000,
        PT_LOAD,          0x3000,  0x403000,  0x04,    0x04,   PF_R | PF_W | PF_X, 0x1000,
        END
    );
}

// This tests an orphan section with an odd name. There is no other section for it to be
// placed afterwards, so it ends up at the end of the file
static void test_orphan_sections_no_rearrangement(void) {
    char *object_path1 = run_was(
        ".text;"
        ".globl _start;"

        "_start:;"
        "    movl $1, %eax;"        // sys_exit
        "    movl i(%rip), %ebx;"
        "    movl j(%rip), %ecx;"
        "    add %ecx, %ebx;"
        "    sub $3, %ebx;"         // exit code
        "    int $0x80;"            // call kernel

        // awx sections are unusual. None of the source files will have thus,
        // so this section ends up being a new section.
        ".section .orphan, \"awx\";"
        "i: .long 1;"
    );

    char *object_path2 = run_was(
        ".globl j;"
        ".section .orphan, \"awx\";"
        "j: .long 2;"
    );

    List *input_paths = new_list(1);
    append_to_list(input_paths, object_path1);
    append_to_list(input_paths, object_path2);

    char *output_path;
    RwElfFile *elf_file = run_wld(input_paths, &output_path, 1, "orphan sections");

    assert_sections(elf_file,
        // Name           Type            Address   Offset  Size   Flags                                  Align
        ".text",          SHT_PROGBITS,   0x401000, 0x1000, 0x20,  SHF_ALLOC | SHF_EXECINSTR,             16,
        ".orphan",        SHT_PROGBITS,   0x402000, 0x2000, 0x08,  SHF_ALLOC | SHF_WRITE | SHF_EXECINSTR, 1,
        NULL
    );

    // The orphan section gets merged into the .data section
    assert_program_segments(elf_file,
        // Type           Offset   VirtAddr   FileSiz  MemSiz  Flags               Align
        PT_LOAD,          0x1000,  0x401000,  0x20,    0x20,   PF_R | PF_X,        0x1000,
        PT_LOAD,          0x2000,  0x402000,  0x08,    0x08,   PF_R | PF_X | PF_W, 0x1000,
        END
    );
}

// This tests a secton with an odd name. There is also a .data section
// with the same permissions. The orphan section should be placed after it.
// The .bss section is to ensure that the .orphan section actually gets moved.
static void test_orphan_sections_rearrangement(void) {
    char *object_path1 = run_was(
        ".text;"
        ".globl _start;"

        "_start:;"
        "    movl $1, %eax;"        // sys_exit
        "    movl i(%rip), %ebx;"
        "    movl j(%rip), %ecx;"
        "    add %ecx, %ebx;"
        "    movl k(%rip), %ecx;"
        "    add %ecx, %ebx;"
        "    sub $6, %ebx;"         // exit code
        "    int $0x80;"            // call kernel

        ".section .bss,\"aw\",@nobits;"
        "    .zero 8;"

        ".section .orphan, \"aw\";"
        "i: .long 1;"
        ".section .data, \"aw\";"
        "j: .long 2;"
    );

    char *object_path2 = run_was(
        ".globl k;"
        ".section .orphan, \"aw\";"
        "k: .long 3;"
    );

    List *input_paths = new_list(1);
    append_to_list(input_paths, object_path1);
    append_to_list(input_paths, object_path2);

    char *output_path;
    RwElfFile *elf_file = run_wld(input_paths, &output_path, 1, "orphan sections");

    assert_sections(elf_file,
        // Name           Type            Address   Offset  Size   Flags                      Align
        ".text",          SHT_PROGBITS,   0x401000, 0x1000, 0x20,  SHF_ALLOC | SHF_EXECINSTR, 16,
        ".data",          SHT_PROGBITS,   0x402000, 0x2000, 0x04,  SHF_ALLOC | SHF_WRITE,     4,
        ".orphan",        SHT_PROGBITS,   0x402004, 0x2004, 0x08,  SHF_ALLOC | SHF_WRITE,     1,
        ".bss",           SHT_NOBITS,     0x40200c, 0x200c, 0x08,  SHF_ALLOC | SHF_WRITE,     4,
        NULL
    );

    // The orphan section gets merged into the .data section
    assert_program_segments(elf_file,
        // Type           Offset   VirtAddr   FileSiz  MemSiz    Flags         Align
        PT_LOAD,          0x1000,  0x401000,  0x20,    0x20,     PF_R | PF_X,  0x1000,  // .text
        PT_LOAD,          0x2000,  0x402000,  0x0c,    0x14,     PF_R | PF_W,  0x1000,  // .data, .bss
        END
    );
}

// Test TLS tdata and tss sections
static void test_tls() {
    char *object_path = run_was(
        ".globl _start;"
        ".text;"
        "_start:;"
        "    movl $1, %eax;"
        "    movl $0, %ebx;"
        "    int $0x80;"
        ".section .tdata,\"awT\",@progbits;"
        "    .long 0;"
        ".section .tbss,\"awT\",@nobits;"
        "    .long 0;"
    );

    List *input_paths = new_list(1);
    append_to_list(input_paths, object_path);

    char *output_path;
    RwElfFile *elf_file = run_wld(input_paths, &output_path, 1, "tls");

    assert_sections(elf_file,
        // Name           Type            Address   Offset  Size   Flags                            Align
        ".text",          SHT_PROGBITS,   0x401000, 0x1000, 0x0c,  SHF_ALLOC | SHF_EXECINSTR,       16,
        ".tdata",         SHT_PROGBITS,   0x402ffc, 0x2ffc, 0x04,  SHF_ALLOC | SHF_WRITE | SHF_TLS, 1,
        ".tbss",          SHT_NOBITS,     0x403000, 0x3000, 0x04,  SHF_ALLOC | SHF_WRITE | SHF_TLS, 1,
        NULL
    );

    assert_program_segments(elf_file,
        // Type           Offset   VirtAddr   FileSiz  MemSiz  Flags         Align
        PT_LOAD,          0x1000,  0x401000,  0x0c,    0x0c,   PF_R | PF_X,  0x1000,
        PT_LOAD,          0x2ffc,  0x402ffc,  0x04,    0x08,   PF_R | PF_W,  0x1000,
        PT_TLS,           0x2ffc,  0x402ffc,  0x04,    0x08,   PF_R       ,  8,
        END
    );
}

// Test merging of two bss sections in different files
static void test_two_bss_sections(void) {
    char *object_path1 = run_was(
        ".text;"
        ".globl _start;"

        "_start:;"
        "    movl $1, %eax;"        // sys_exit
        "    movl $0, %ebx;"        // exit code
        "    int $0x80;"            // call kernel
        ".section .bss,\"aw\",@nobits;"
        "    .zero 8;"
    );

    char *object_path2 = run_was(
        ".section .bss,\"aw\",@nobits;"
        "    .zero 8;"
    );

    List *input_paths = new_list(1);
    append_to_list(input_paths, object_path1);
    append_to_list(input_paths, object_path2);

    char *output_path;
    RwElfFile *elf_file = run_wld(input_paths, &output_path, 1, "two bss sections");

    assert_sections(elf_file,
        // Name           Type            Address   Offset  Size   Flags                      Align
        ".text",          SHT_PROGBITS,   0x401000, 0x1000, 0x10,  SHF_ALLOC | SHF_EXECINSTR, 16,
        ".bss",           SHT_NOBITS,     0x402000, 0x2000, 0x10,  SHF_ALLOC | SHF_WRITE,     4,
        NULL
    );

    // The orphan section gets merged into the .data section
    assert_program_segments(elf_file,
        // Type           Offset   VirtAddr   FileSiz  MemSiz  Flags        Align
        PT_LOAD,          0x1000,  0x401000,  0x10,    0x10,   PF_R | PF_X, 0x1000,
        PT_LOAD,          0x2000,  0x402000,  0x00,    0x10,   PF_R | PF_W, 0x1000,
        END
    );
}

// Test merging of data and two bss sections in different files
static void test_data_and_two_bss_sections(void) {
    char *object_path1 = run_was(
        ".text;"
        ".globl _start;"

        "_start:;"
        "    movl $1, %eax;"        // sys_exit
        "    movl $0, %ebx;"        // exit code
        "    int $0x80;"            // call kernel
        ".data;"
        "    .zero 8;"
        ".section .bss,\"aw\",@nobits;"
        "    .zero 8;"
    );

    char *object_path2 = run_was(
        ".section .bss,\"aw\",@nobits;"
        "    .zero 8;"
    );

    List *input_paths = new_list(1);
    append_to_list(input_paths, object_path1);
    append_to_list(input_paths, object_path2);

    char *output_path;
    RwElfFile *elf_file = run_wld(input_paths, &output_path, 1, "two bss sections");

    assert_sections(elf_file,
        // Name           Type            Address   Offset  Size   Flags                      Align
        ".text",          SHT_PROGBITS,   0x401000, 0x1000, 0x10,  SHF_ALLOC | SHF_EXECINSTR, 16,
        ".data",          SHT_PROGBITS,   0x402000, 0x2000, 0x08,  SHF_ALLOC | SHF_WRITE,     4,
        ".bss",           SHT_NOBITS,     0x402008, 0x2008, 0x10,  SHF_ALLOC | SHF_WRITE,     4,
        NULL
    );

    // The orphan section gets merged into the .data section
    assert_program_segments(elf_file,
        // Type           Offset   VirtAddr   FileSiz  MemSiz  Flags        Align
        PT_LOAD,          0x1000,  0x401000,  0x10,    0x10,   PF_R | PF_X, 0x1000,
        PT_LOAD,          0x2000,  0x402000,  0x08,    0x18,   PF_R | PF_W, 0x1000,
        END
    );
}


// Test reading etext without defining it. The linker adds it.
static void test_etext_undefined() {
    char *object_path = run_was(
        ".globl _start;"
        ".text;"
        "_start:;"
        "    lea etext(%rip), %rdi;"
        "    movl $1, %eax;"
        "    movl $0, %ebx;"
        "    int $0x80;"
    );

    List *input_paths = new_list(1);
    append_to_list(input_paths, object_path);

    char *output_path;
    RwElfFile *elf_file = run_wld(input_paths, &output_path, 1, "undefined etext");

    assert_sections(elf_file,
        // Name           Type            Address   Offset  Size   Flags                      Align
        ".text",          SHT_PROGBITS,   0x401000, 0x1000, 0x13,  SHF_ALLOC | SHF_EXECINSTR, 16,
        NULL
    );

    assert_program_segments(elf_file, //
        // Type           Offset   VirtAddr   FileSiz  MemSiz  Flags         Align
        PT_LOAD,          0x1000,  0x401000,  0x13,    0x13,   PF_R | PF_X,  0x1000,
        END
    );

    assert_symbols(elf_file,
    //  Value      Size   Type        Binding     Visibility   Section   Name
        0,         0,     STT_NOTYPE, STB_GLOBAL, STV_HIDDEN,   "ABS",   "_GLOBAL_OFFSET_TABLE_",
        0x401000,  0,     STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, ".text", "_start",
        0x401013,  0,     STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, "ABS",   "etext",                    // Set by linker
        END);
}

// Test reading etext from a definition, so the linker doesn't override it
static void test_defined_etext() {
    char *object_path = run_was(
        ".globl _start;"
       ".globl etext;"
        ".text;"
        "_start:;"
        "    lea etext(%rip), %rdi;"
        "    movl $1, %eax;"
        "    movl $0, %ebx;"
        "    int $0x80;"
        ".section .data;"
        ".zero 0x1010;"    // Some padding; so that we can assert etext isn't set after .text by the linker
        "etext: .long 1"
    );

    List *input_paths = new_list(1);
    append_to_list(input_paths, object_path);

    char *output_path;
    RwElfFile *elf_file = run_wld(input_paths, &output_path, 1, "defined etext");

    assert_sections(elf_file,
        // Name           Type            Address   Offset  Size     Flags                      Align
        ".text",          SHT_PROGBITS,   0x401000, 0x1000, 0x0013,  SHF_ALLOC | SHF_EXECINSTR, 16,
        ".data",          SHT_PROGBITS,   0x402000, 0x2000, 0x1014,  SHF_ALLOC | SHF_WRITE,     4,
        NULL
    );

    assert_program_segments(elf_file, //
        // Type           Offset   VirtAddr   FileSiz  MemSiz  Flags         Align
        PT_LOAD,          0x1000,  0x401000,  0x0013,  0x0013, PF_R | PF_X,  0x1000,
        PT_LOAD,          0x2000,  0x402000,  0x1014,  0x1014, PF_R | PF_W, 0x1000,
        END
    );

    assert_symbols(elf_file,
    //  Value      Size   Type        Binding     Visibility   Section   Name
        0,         0,     STT_NOTYPE, STB_GLOBAL, STV_HIDDEN,  "ABS",   "_GLOBAL_OFFSET_TABLE_",
        0x403010,  0,     STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, ".data",  "etext",
        0x401000,  0,     STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, ".text", "_start",
        END);
}

int main() {
    test_sanity();
    test_segments_are_page_aligned();
    test_orphan_sections_rearrangement();
    test_orphan_sections_no_rearrangement();
    test_tls();
    test_two_bss_sections();
    test_data_and_two_bss_sections();
    test_etext_undefined();
    test_defined_etext();
}
