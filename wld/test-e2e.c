#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "list.h"

#include "wld/wld.h"

#define PT_END -1

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
        if (type == PT_END) break;

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
    if (result) { perror("was failed"); exit(1); }

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
        PT_END
    );
}

// Test an oddball section with different flags after a data section.
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
        ".section .oddball, \"a\", @progbits;"
        "    .long 0;"
    );

    List *input_paths = new_list(1);
    append_to_list(input_paths, object_path);

    char *output_path;
    RwElfFile *elf_file = run_wld(input_paths, &output_path, 1, "sanity");

    assert_sections(elf_file,
        // Name           Type            Address   Offset  Size   Flags                      Align
        ".text",          SHT_PROGBITS,   0x401000, 0x1000, 0x10,  SHF_ALLOC | SHF_EXECINSTR, 16,
        ".data",          SHT_PROGBITS,   0x402000, 0x2000, 0x04,  SHF_ALLOC | SHF_WRITE,     4,
        ".oddball",       SHT_PROGBITS,   0x403000, 0x3000, 0x04,  SHF_ALLOC,                 1,
        NULL
    );

    assert_program_segments(elf_file,
        // Type           Offset   VirtAddr   FileSiz  MemSiz  Flags         Align
        PT_LOAD,          0x1000,  0x401000,  0x10,    0x10,   PF_R | PF_X,  0x1000,
        PT_LOAD,          0x2000,  0x402000,  0x04,    0x04,   PF_R | PF_W,  0x1000,
        PT_LOAD,          0x3000,  0x403000,  0x04,    0x04,   PF_R,         0x1000,
        PT_END
    );
}

int main() {
    test_sanity();
    test_segments_are_page_aligned();
}
