#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "list.h"
#include "error.h"

#include "wld/relocations.h"
#include "wld/symbols.h"
#include "wld/wld.h"

#define END -1

static void assert_int(int expected, int actual, const char *message) {
    if (expected != actual) {
        printf("%s: expected %d, got %d\n", message, expected, actual);
        exit(1);
    }
}

static void assert_uint64_t(uint64_t expected, uint64_t actual, const char *message) {
    if (expected != actual) {
        printf("%s: expected %lx, got %lx\n", message, expected, actual);
        exit(1);
    }
}

static void assert_string(const char *expected, const char *actual, const char *message) {
    if (strcmp(expected, actual)) {
        printf("%s: expected %s, got %s\n", message, expected, actual);
        exit(1);
    }
}

static void fail_sections(OutputElfFile *elf_file, List *expected_sections) {
    printf("Got sections:\n");
    dump_sections(elf_file);

    // Brutally clobber the ELF file's sections. We're bailing anyways so who cares.
    printf("\nExpected sections:\n");
    elf_file->sections_list = expected_sections;
    dump_sections(elf_file);
    exit(1);
}

static void assert_sections(OutputElfFile *elf_file, ...) {
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

        OutputSection *section = calloc(1, sizeof(OutputSection));
        section->name = strdup(name);
        section->type = type;
        section->address = address;
        section->offset = offset;
        section->size = size;
        section->flags = flags;
        section->align = align;

        append_to_list(expected_sections, section);
    }

    // The executable has 4 extra sections: null, symtab, strtab and shstrtab
    if (elf_file->sections_list->length - 4 != expected_sections->length) {
        printf("Sections lengths mismatch: expected %d, got %d\n", elf_file->sections_list->length - 4, expected_sections->length);
        fail_sections(elf_file, expected_sections);
    }

    for (int i = 0; i < expected_sections->length; i++) {
        OutputSection *got = elf_file->sections_list->elements[i + 1];
        OutputSection *expected = expected_sections->elements[i];

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

static void fail_program_segments(OutputElfFile *elf_file, List *expected_program_segments) {
    printf("Got program_segments:\n");
    dump_program_segments(elf_file);

    // Brutally clobber the ELF file's program_segments. We're bailing anyways so who cares.
    printf("\nExpected program_segments:\n");
    elf_file->program_segments_list = expected_program_segments;
    dump_program_segments(elf_file);
    exit(1);
}

static void assert_program_segments(OutputElfFile *elf_file, ...) {
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

static void assert_symbol_table(OutputElfFile *elf_file, OutputSection *section, va_list ap) {
    int pos = sizeof(ElfSymbol); // Skip null symbol

    while (1) {
        int expected_value = va_arg(ap, int);

        if (expected_value == END) {
            if (pos != section->size) {
                dump_output_symbols(elf_file);
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
        else if (!strcmp(expected_section_name, "UND")) {
            expected_section_index = SHN_UNDEF;
        }
        else {
            OutputSection *expected_section = get_output_section(elf_file, expected_section_name);
            if (!expected_section) {
                dump_output_symbols(elf_file);
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

        OutputSection *str_section = elf_file->sections_list->elements[section->link];
        char *strings = str_section->data;
        char *got_name = symbol->st_name ? &strings[symbol->st_name] : NULL;

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

            printf("%s mismatch\n"
                "got      value=%#08x  size=%#08x type=%d binding=%d visiblility=%d index=%5d name=%s\n"
                "expected value=%#08x  size=%#08x type=%d binding=%d visiblility=%d index=%5d name=%s\n",
                section->name,


                got_value,
                got_size,
                got_type,
                got_binding,
                got_visibility,
                got_index,
                got_name ? got_name : "null",

                expected_value,
                expected_size,
                expected_type,
                expected_binding,
                expected_visibility,
                expected_section_index,
                expected_name ? expected_name : "null"

            );

            exit(1);
        }

        pos += sizeof(ElfSymbol);
    }
}

static void assert_symtab(OutputElfFile *elf_file, ...) {
    OutputSection *section = elf_file->section_symtab;
    if (!section) panic("Expected a symtab section");

    va_list ap;
    va_start(ap, elf_file);

    assert_symbol_table(elf_file, section, ap);
}

static void assert_dynsym(OutputElfFile *elf_file, ...) {
    OutputSection *section = elf_file->section_dynsym->output_section;
    if (!section) panic("Expected a dynsym section");

    va_list ap;
    va_start(ap, elf_file);

    assert_symbol_table(elf_file, section, ap);
}

static void assert_dynamic(OutputElfFile *elf_file, ...) {
    OutputSection *dynamic_section = get_output_section(elf_file, DYNAMIC_SECTION_NAME);
    if (!dynamic_section) panic("No .dynamic section");

    OutputSection *dynstr_section = get_output_section(elf_file, DYNSTR_SECTION_NAME);
    if (!dynstr_section) panic("No .dynstr section");

    uint64_t length = dynamic_section->size / sizeof(ElfDyn);
    int pos = 0;

    va_list ap;
    va_start(ap, elf_file);

    while (1) {
        int expected_tag = va_arg(ap, int);
        int expected_value = va_arg(ap, int);
        char *expected_dynstr = va_arg(ap, char *);

        ElfDyn *dyn = &((ElfDyn *) dynamic_section->data)[pos];
        uint64_t got_tag = dyn->d_tag & 0xffffffff;
        uint64_t got_value = dyn->d_un.d_val;

        char *got_dynstr = NULL;
        if (expected_dynstr) {
            got_dynstr = &dynstr_section->data[got_value];
        }

        const char *expected_tag_name = dynamic_section_name(expected_tag);
        const char *got_tag_name = section_type_name(got_tag);

        int value_matches = expected_dynstr
            ? expected_dynstr && !strcmp(expected_dynstr, got_dynstr)
            : expected_value == got_value;

        if (expected_tag != got_tag || !value_matches) {
            dump_dynamic_section(elf_file);
            printf("Mismatch at position %d\n"
                "Expected tag:   %#x , got tag:   %#lx\n"
                "Expected value: %#x,  got value: %#lx\n"
                "Expected dynstr: %s,  got dynstr: %s\n",
                pos,
                expected_tag, got_tag,
                expected_value, got_value,
                expected_dynstr, got_dynstr);
            exit(1);
        }

        if (expected_tag == DT_NULL) {
            if (pos != length - 1) {
                dump_dynamic_section(elf_file);
                panic("Unexpected data at position %d", pos);
            }

            return; // Success
        }

        pos++;
    }
}

void assert_relocations(OutputElfFile *elf_file, OutputSection *section, va_list ap) {
    if (!section) panic("Assert section on a NULL");

    int pos = 0;

    while (1) {
        int expected_type           = va_arg(ap, int);
        int expected_symbol_index   = va_arg(ap, int);
        int expected_offset         = va_arg(ap, int);
        int expected_addend         = va_arg(ap, int);

        if (expected_type == END) {
            if (pos != section->size) {
                dump_relocations(section);
                panic("Unexpected data at position %d", pos / sizeof(ElfRelocation));
            }

            return; // Success
        }

        ElfRelocation *r = (ElfRelocation *) &section->data[pos];

        int got_type         = r->r_info & -1;
        int got_symbol_index = r->r_info >> 32;
        int got_offset       = r->r_offset;
        int got_addend       = r->r_addend;

        if (pos == section->size) {
            dump_relocations(section);
            panic("Expected extra data at position %d", pos / sizeof(ElfRelocation));
        }

        if (
                expected_type != got_type ||
                expected_symbol_index != got_symbol_index ||
                expected_offset != got_offset ||
                expected_addend != got_addend) {

            dump_relocations(section);

            const char *expected_type_name = expected_type < RELOCATION_NAMES_COUNT ? RELOCATION_NAMES[expected_type] : "UNKNOWN";
            const char *got_type_name = got_type < RELOCATION_NAMES_COUNT ? RELOCATION_NAMES[got_type] : "UNKNOWN";


            panic("Relocations mismatch at position %d:\n"
                "got      type=%30s, symbol index=%4d, offset=%#08x, addend=%#08lx\n"
                "expected type=%30s, symbol index=%4d, offset=%#08x, addend=%#08lx\n",
                pos / sizeof(ElfRelocation),
                got_type_name,
                got_symbol_index,
                got_offset,
                got_addend,
                expected_type_name,
                expected_symbol_index,
                expected_offset,
                expected_addend
            );
        }

        pos += sizeof(ElfRelocation);
    }
}

void assert_rela_dyn_relocations(OutputElfFile *elf_file, ...) {
    OutputSection *section = get_output_section(elf_file, RELA_DYN_SECTION_NAME);
    if (!section) panic("No .rela.dyn section\n");

    va_list ap;
    va_start(ap, elf_file);

    assert_relocations(elf_file, section, ap);
}

void assert_rela_plt_relocations(OutputElfFile *elf_file, ...) {
    OutputSection *section = get_output_section(elf_file, RELA_PLT_SECTION_NAME);
    if (!section) panic("No .rela.plt section\n");

    va_list ap;
    va_start(ap, elf_file);

    assert_relocations(elf_file, section, ap);
}

// Print hex bytes
static int hexdump(char *data, int size) {
    for (int i = 0; i < size; i++) {
        if (i != 0) printf(", ");
        printf("0x%02x", (unsigned char) data[i]);
    }
    printf("\n");
}

static void vassert_data(char *data, int size, va_list ap) {
    int pos = 0;

    while (1) {
        unsigned int expected = va_arg(ap, unsigned int);

        if (expected == END) {
            if (pos != size) {
                hexdump(data, size);
                panic("Unexpected data at position %d", pos);
            }

            return; // Success
        }

        if (pos == size) {
            hexdump(data, size);
            panic("Expected extra data at position %d: %#02x", pos, expected & 0xff);
        }

        if ((expected & 0xff) != (data[pos] & 0xff)) {
            hexdump(data, size);
            panic("Mismatch at position %d: expected %#02x, got %#02x", pos, (uint8_t) expected & 0xff, data[pos] & 0xff);
        }

        pos++;
    }
}

static void vassert_section_data(OutputSection* section, va_list ap) {
    if (!section) panic("Assert section on a NULL");
    vassert_data(section->data, section->size, ap);
}

static void assert_section_data(OutputElfFile *elf_file, const char *section_name, ...) {
    va_list ap;
    va_start(ap, section_name);

    OutputSection* output_section = get_output_section(elf_file, section_name);
    if (!output_section) panic("Did not find %s", section_name);
    vassert_section_data(output_section, ap);
    va_end(ap);
}

static void *write_file(const char *filename, const char *contents) {
    FILE *f = fopen(filename, "w");
    if (!f) panic("Could not write %s\n", filename);
    // Write the string to the file
    fprintf(f, "%s", contents);
    fclose(f);
}

// Run GNU as. This is fragile as GNU as output can't be expected to be consistent
// in different versions.
static char *run_as(char *extra_args, char *assembly) {
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
    // snprintf(command, sizeof(command), "as -o %s %s", object_path, source_path);
    snprintf(command, sizeof(command), "as -mrelax-relocations=no -o %s %s %s",
        object_path, source_path,
        extra_args ? extra_args : ""
    );

    int result = system(command);
    if (result) {
        printf("Was failed with exit code %d\n", result >> 8);
        exit(1);
    }

    return strdup(object_path);
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

static char *run_wld_and_capture_stderr(List *input_filenames, List *extra_args, const char *output_path) {
    int pipe_fds[2];
    if (pipe(pipe_fds) == -1) { perror("pipe"); exit(1); }

    pid_t pid = fork();
    if (pid == -1) { perror("fork"); exit(1); }

    if (pid == 0) {
        close(pipe_fds[0]);
        if (dup2(pipe_fds[1], STDERR_FILENO) == -1) { perror("dup2"); exit(1); }
        close(pipe_fds[1]);

        int argc = 0;
        int max_args = input_filenames->length + 8;
        if (extra_args) max_args += extra_args->length;
        char **argv = calloc(max_args, sizeof(char *));
        if (!argv) { perror("calloc"); exit(1); }

        argv[argc++] = "../bin/wld";
        argv[argc++] = "-L";
        argv[argc++] = "/tmp/";
        argv[argc++] = "-o";
        argv[argc++] = (char *) output_path;

        for (int i = 0; i < input_filenames->length; i++) {
            char *input_filename = input_filenames->elements[i];

            if (input_filename[0] == '*') {
                // Special case for library filenames, they start with an '*'
                // Convert that to "-lxyz"
                char *library_arg = malloc(strlen(input_filename + 2));
                library_arg[0] = '-';
                library_arg[1] = 'l';
                strcpy(&library_arg[2], &input_filename[1]);
                argv[argc++] = library_arg;
            }
            else {
                argv[argc++] = input_filename;
            }
        }

        if (extra_args) {
            for (int i = 0; i < extra_args->length; i++) argv[argc++] = extra_args->elements[i];
        }

        argv[argc] = NULL;

        execv(argv[0], argv);
        perror("execv");
        _exit(1);
    }

    close(pipe_fds[1]);

    size_t capacity = 1024;
    size_t length = 0;
    char *result = malloc(capacity);
    if (!result) { perror("malloc"); exit(1); }

    char buffer[256];
    ssize_t read_bytes = 0;
    while ((read_bytes = read(pipe_fds[0], buffer, sizeof(buffer))) > 0) {
        if (length + read_bytes + 1 > capacity) {
            while (length + read_bytes + 1 > capacity) capacity *= 2;
            result = realloc(result, capacity);
            if (!result) { perror("realloc"); exit(1); }
        }
        memcpy(result + length, buffer, read_bytes);
        length += read_bytes;
    }

    result[length] = '\0';

    close(pipe_fds[0]);

    int status = 0;
    if (waitpid(pid, &status, 0) == -1) { perror("waitpid"); exit(1); }

    int exit_code = WEXITSTATUS(status);
    if (exit_code != 1)
        panic("Wld exited with unexpected exit code %d, expected 1:\n%s", exit_code, result);

    return result;
}


static OutputElfFile *run_wld(List *input_filenames, int output_type, char **poutput_lib_name, int run_executable, char *test_name) {
    // Make list of InputFile from the input filenames
    List *input_files = new_list(input_filenames->length);
    for (int i = 0; i < input_filenames->length; i++) {
        char *filename = input_filenames->elements[i];
        InputFile *input_file = malloc(sizeof(InputFile));

        int is_library = 0;
        if (filename[0] == '*') {
            filename++;
            is_library = 1;
        }

        input_file->filename = strdup(filename);
        input_file->is_library = is_library;
        append_to_list(input_files, input_file);
    }

    char template[] = "XXXXXX";
    int fd = mkstemp(template);
    if (fd == -1) { perror("mkstemp"); exit(1); }
    close(fd);
    unlink(template);

    char *output_path;
    if (output_type & OUTPUT_TYPE_FLAG_SHARED && !(output_type & OUTPUT_TYPE_FLAG_EXECUTABLE)) {
        // Append .so
        output_path = malloc(strlen(template) + 32);
        sprintf(output_path, "/tmp/libwld%s.so", template);

        if (!poutput_lib_name) panic("Need to set poutput_lib_name");
        char *output_lib_name = malloc(strlen(template) + 32);
        sprintf(output_lib_name, "*wld%s", template); // The leading * tells run_wld it's a library
        *poutput_lib_name = output_lib_name;
    }
    else {
        output_path = malloc(strlen(template) + 32);
        sprintf(output_path, "/tmp/wld%s", template);
    }

    List *linker_scripts = new_list(0);

    List *library_paths = new_list(0);
    append_to_list(library_paths, "/tmp");

    OutputElfFile *elf_file = run(library_paths, linker_scripts, input_files, output_path, output_type, NULL);

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

    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_STATIC | OUTPUT_TYPE_FLAG_EXECUTABLE, NULL, 1, "sanity");

    assert_sections(elf_file,
        // Name           Type            Address   Offset  Size   Flags                      Align
        ".text",          SHT_PROGBITS,   0x401000, 0x1000, 0x10,  SHF_ALLOC | SHF_EXECINSTR, 16,
        ".data",          SHT_PROGBITS,   0x402000, 0x2000, 0x04,  SHF_ALLOC | SHF_WRITE,     4,
        NULL
    );

    assert_program_segments(elf_file,
        // Type           Offset   VirtAddr   FileSiz  MemSiz  Flags         Align
        PT_LOAD,          0x0000,  0x400000,  0x180,   0x180,  PF_R,         0x1000,
        PT_LOAD,          0x1000,  0x401000,  0x010,   0x10,   PF_R | PF_X,  0x1000,
        PT_LOAD,          0x2000,  0x402000,  0x004,   0x04,   PF_R | PF_W,  0x1000,
        END
    );

    assert_symtab(elf_file,
    //  Value      Size   Type        Binding     Visibility   Section   Name
        0x402000,  0,     STT_NOTYPE, STB_LOCAL,  STV_DEFAULT, ".data",  "code",
        0x401000,  0,     STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, ".text",  "_start",
        END);
}

// Test a silly edge case where an object file could be empty
static void test_empty_object_file() {
    char *object1_path = run_was(
        ".globl _start;"
        ".text;"
        "_start: nop;"
    );

    char *object2_path = run_was("");

    List *input_paths = new_list(1);
    append_to_list(input_paths, object1_path);
    append_to_list(input_paths, object2_path);

    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_STATIC | OUTPUT_TYPE_FLAG_EXECUTABLE, NULL, 0, "sanity");

    assert_sections(elf_file,
        // Name           Type            Address   Offset  Size   Flags                      Align
        ".text",          SHT_PROGBITS,   0x401000, 0x1000, 0x10,  SHF_ALLOC | SHF_EXECINSTR, 16,
        NULL
    );
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

    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_STATIC | OUTPUT_TYPE_FLAG_EXECUTABLE,  NULL, 1, "sanity");

    assert_sections(elf_file,
        // Name           Type            Address   Offset  Size   Flags                                  Align
        ".text",          SHT_PROGBITS,   0x401000, 0x1000, 0x10,  SHF_ALLOC | SHF_EXECINSTR,             16,
        ".data",          SHT_PROGBITS,   0x402000, 0x2000, 0x04,  SHF_ALLOC | SHF_WRITE,                 4,
        ".orphan",        SHT_PROGBITS,   0x403000, 0x3000, 0x04,  SHF_ALLOC | SHF_EXECINSTR | SHF_WRITE, 1,
        NULL
    );

    assert_program_segments(elf_file,
        // Type           Offset   VirtAddr   FileSiz  MemSiz  Flags               Align
        PT_LOAD,          0x0000,  0x400000,  0x1c0,   0x1c0,  PF_R,               0x1000,
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

    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_STATIC | OUTPUT_TYPE_FLAG_EXECUTABLE,  NULL, 1, "orphan sections");

    assert_sections(elf_file,
        // Name           Type            Address   Offset  Size   Flags                                  Align
        ".text",          SHT_PROGBITS,   0x401000, 0x1000, 0x20,  SHF_ALLOC | SHF_EXECINSTR,             16,
        ".orphan",        SHT_PROGBITS,   0x402000, 0x2000, 0x08,  SHF_ALLOC | SHF_WRITE | SHF_EXECINSTR, 1,
        NULL
    );

    // The orphan section gets merged into the .data section
    assert_program_segments(elf_file,
        // Type           Offset   VirtAddr   FileSiz  MemSiz  Flags               Align
        PT_LOAD,          0x0000,  0x400000,  0x180,   0x180,  PF_R,               0x1000,
        PT_LOAD,          0x1000,  0x401000,  0x020,   0x020,  PF_R | PF_X,        0x1000,
        PT_LOAD,          0x2000,  0x402000,  0x008,   0x008,  PF_R | PF_X | PF_W, 0x1000,
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

    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_STATIC | OUTPUT_TYPE_FLAG_EXECUTABLE,  NULL, 1, "orphan sections");

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
        PT_LOAD,          0x0000,  0x400000,  0x200,   0x200,    PF_R,         0x1000,
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

    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_STATIC | OUTPUT_TYPE_FLAG_EXECUTABLE,  NULL, 1, "tls");

    assert_sections(elf_file,
        // Name           Type            Address   Offset  Size   Flags                            Align
        ".text",          SHT_PROGBITS,   0x401000, 0x1000, 0x0c,  SHF_ALLOC | SHF_EXECINSTR,       16,
        ".tdata",         SHT_PROGBITS,   0x402ffc, 0x2ffc, 0x04,  SHF_ALLOC | SHF_WRITE | SHF_TLS, 1,
        ".tbss",          SHT_NOBITS,     0x403000, 0x3000, 0x04,  SHF_ALLOC | SHF_WRITE | SHF_TLS, 1,
        NULL
    );

    assert_program_segments(elf_file,
        // Type           Offset   VirtAddr   FileSiz  MemSiz  Flags         Align
        PT_LOAD,          0x0000,  0x400000,  0x1c0,   0x1c0,  PF_R,         0x1000,
        PT_LOAD,          0x1000,  0x401000,  0x0c,    0x0c,   PF_R | PF_X,  0x1000,
        PT_LOAD,          0x2ffc,  0x402ffc,  0x04,    0x08,   PF_R | PF_W,  0x1000,
        PT_TLS,           0x2ffc,  0x402ffc,  0x04,    0x08,   PF_R ,        8,
        END
    );
}

// Test the start of the TLS template is a multiple of the TLS section's alignment
static void test_tls_aligment() {
    char *object_path1 = run_as(NULL,
        "    .globl	data2;"
        "    .section .tdata,\"awT\",@progbits;"
        "    .align 8;"
        "    .type data2, @object;"
        "    .size data2, 8;"
        "data2: .quad 43"
    );

    char *object_path2 = run_as(NULL,
        "    .globl	data1;"
        "    .section .tdata,\"awT\",@progbits;"
        "    .align 4;"
        "    .type data1, @object;"
        "    .size data1, 4;"
        "data1: .long 42;"
        ".globl _start;"
        ".text;"
        "_start:;"
	    "movl %fs:data1@tpoff, %eax;"
	    "movq data2@gottpoff(%rip), %rax;"
        "movq %fs:(%rax), %rax"
    );

    List *input_paths = new_list(1);
    append_to_list(input_paths, object_path1);
    append_to_list(input_paths, object_path2);

    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_STATIC | OUTPUT_TYPE_FLAG_EXECUTABLE,  NULL, 0, "test_tls_aligment");

    assert_sections(elf_file,
        // Name           Type            Address   Offset  Size   Flags                            Align
        ".text",          SHT_PROGBITS,   0x401000, 0x1000, 0x13,  SHF_ALLOC | SHF_EXECINSTR,       1,
        ".tdata",         SHT_PROGBITS,   0x402ff0, 0x2ff0, 0x0c,  SHF_ALLOC | SHF_WRITE | SHF_TLS, 8,
        NULL
    );

    assert_program_segments(elf_file,
        // Type           Offset   VirtAddr   FileSiz  MemSiz  Flags         Align
        PT_LOAD,          0x0000,  0x400000,  0x180,   0x180,  PF_R,         0x1000,
        PT_LOAD,          0x1000,  0x401000,  0x13,    0x13,   PF_R | PF_X,  0x1000,
        PT_LOAD,          0x2ff0,  0x402ff0,  0x0c,    0x0c,   PF_R | PF_W,  0x1000,
        PT_TLS,           0x2ff0,  0x402ff0,  0x0c,    0x0c,   PF_R ,        8,
        END
    );

    // The TLS section size is 12, and alignment 8. This gets aligned to 16,
    // so that he start of the TLS section is 16 bytes below the thread pointer.
    assert_section_data(elf_file, ".text",
        0x64, 0x8b, 0x04, 0x25, 0xf8, 0xff, 0xff, 0xff,     // mov %fs:-8,%eax          data1 is at the thread pointer -8
        0x48, 0xc7, 0xc0, 0xf0, 0xff, 0xff, 0xff,           // mov $-16,%rax            data2 is at the thread pointer -16
        0x64, 0x48, 0x8b, 0x00,                             // mov %fs:(%rax),%rax
        END);
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

    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_STATIC | OUTPUT_TYPE_FLAG_EXECUTABLE,  NULL, 1, "two bss sections");

    assert_sections(elf_file,
        // Name           Type            Address   Offset  Size   Flags                      Align
        ".text",          SHT_PROGBITS,   0x401000, 0x1000, 0x10,  SHF_ALLOC | SHF_EXECINSTR, 16,
        ".bss",           SHT_NOBITS,     0x402000, 0x2000, 0x10,  SHF_ALLOC | SHF_WRITE,     4,
        NULL
    );

    // The orphan section gets merged into the .data section
    assert_program_segments(elf_file,
        // Type           Offset   VirtAddr   FileSiz  MemSiz  Flags        Align
        PT_LOAD,          0x0000,  0x400000,  0x180,   0x180,  PF_R,        0x1000,
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

    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_STATIC | OUTPUT_TYPE_FLAG_EXECUTABLE,  NULL, 1, "two bss sections");

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
        PT_LOAD,          0x0000,  0x400000,  0x1c0,   0x1c0,  PF_R,        0x1000,
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

    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_STATIC | OUTPUT_TYPE_FLAG_EXECUTABLE,  NULL, 1, "undefined etext");

    assert_sections(elf_file,
        // Name           Type            Address   Offset  Size   Flags                      Align
        ".text",          SHT_PROGBITS,   0x401000, 0x1000, 0x13,  SHF_ALLOC | SHF_EXECINSTR, 16,
        NULL
    );

    assert_program_segments(elf_file,
        // Type           Offset   VirtAddr   FileSiz  MemSiz  Flags         Align
        PT_LOAD,          0x0000,  0x400000,  0x140,   0x140,  PF_R,         0x1000,
        PT_LOAD,          0x1000,  0x401000,  0x13,    0x13,   PF_R | PF_X,  0x1000,
        END
    );

    assert_symtab(elf_file,
    //  Value      Size   Type        Binding     Visibility    Section  Name
        0x401000,  0,     STT_NOTYPE, STB_GLOBAL, STV_DEFAULT,  ".text", "_start",
        0x401013,  0,     STT_NOTYPE, STB_GLOBAL, STV_DEFAULT,  "ABS",   "etext",                    // Set by linker
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

    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_STATIC | OUTPUT_TYPE_FLAG_EXECUTABLE,  NULL, 1, "defined etext");

    assert_sections(elf_file,
        // Name           Type            Address   Offset  Size     Flags                      Align
        ".text",          SHT_PROGBITS,   0x401000, 0x1000, 0x0013,  SHF_ALLOC | SHF_EXECINSTR, 16,
        ".data",          SHT_PROGBITS,   0x402000, 0x2000, 0x1014,  SHF_ALLOC | SHF_WRITE,     4,
        NULL
    );

    assert_program_segments(elf_file,
        // Type           Offset   VirtAddr   FileSiz  MemSiz  Flags         Align
        PT_LOAD,          0x0000,  0x400000,  0x180,   0x180,  PF_R,         0x1000,
        PT_LOAD,          0x1000,  0x401000,  0x0013,  0x0013, PF_R | PF_X,  0x1000,
        PT_LOAD,          0x2000,  0x402000,  0x1014,  0x1014, PF_R | PF_W,  0x1000,
        END
    );

    assert_symtab(elf_file,
    //  Value      Size   Type        Binding     Visibility   Section   Name
        0x401000,  0,     STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, ".text", "_start",
        0x403010,  0,     STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, ".data",  "etext",
        END);
}

// Test doing nothing with etext.  This tests a case where an assignment could fail
// due to the symbol being undefined.
static void test_unused_etext() {
    char *object_path = run_was(
        ".globl _start;"
        ".text;"
        "_start:;"
        "    movl $1, %eax;"
        "    movl $0, %ebx;"
        "    int $0x80;"
        ".section .preinit_array, \"aw\", @progbits;"
        ".long 1"
    );

    List *input_paths = new_list(1);
    append_to_list(input_paths, object_path);

    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_STATIC | OUTPUT_TYPE_FLAG_EXECUTABLE,  NULL, 1, "unused etext");

    assert_sections(elf_file,
        // Name            Type            Address   Offset  Size     Flags                      Align
        ".text",           SHT_PROGBITS,   0x401000, 0x1000, 0x000c,  SHF_ALLOC | SHF_EXECINSTR, 16,
        ".preinit_array",  SHT_PROGBITS,   0x402000, 0x2000, 0x0004,  SHF_ALLOC | SHF_WRITE,     1,
        NULL
    );

    assert_program_segments(elf_file,
        // Type           Offset   VirtAddr   FileSiz  MemSiz  Flags         Align
        PT_LOAD,          0x0000,  0x400000,  0x180,   0x180,  PF_R,         0x1000,
        PT_LOAD,          0x1000,  0x401000,  0x000c,  0x000c, PF_R | PF_X,  0x1000,
        PT_LOAD,          0x2000,  0x402000,  0x0004,  0x0004, PF_R | PF_W,  0x1000,
        END
    );

    assert_symtab(elf_file,
    //  Value      Size   Type        Binding     Visibility   Section  Name
        0x401000,  0,     STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, ".text", "_start",
        END);
}

// Test undefined symbol error output
static void test_undefined_symbol() {
    char *object_path = run_was(
        ".text;"
        ".globl _start;"
        "_start: callq f@PLT;"
    );

    List *input_paths = new_list(1);
    append_to_list(input_paths, object_path);

    char output_template[] = "/tmp/asm_XXXXXX";
    int fd = mkstemp(output_template);
    if (fd == -1) { perror("mkstemp"); exit(1); }
    close(fd);

    char *stderr_output = run_wld_and_capture_stderr(input_paths, NULL, output_template);

    const char *expected =
        "Undefined symbols:\n"
        "  f\n"
        "error: Unable to resolve undefined references\n";

    assert_string(expected, stderr_output, "undefined symbol in plt call");
    free(stderr_output);
}

// Test adding of __start_ and __stop_ symbols for sections with C names
static void test_automatic_start_stop_symbols() {
    char *object_path = run_was(
        ".globl _start;"
        ".text;"
        "_start:;"
        "    lea __start_foo(%rip), %rdi;" // Use the symbols, so they make it to the symbol table
        "    lea __stop_foo(%rip), %rdi;"
        "    movl $1, %eax;"
        "    movl $0, %ebx;"
        "    int $0x80;"
        ".section foo, \"aw\", @progbits;"
        "   .long 1"
    );

    List *input_paths = new_list(1);
    append_to_list(input_paths, object_path);

    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_STATIC | OUTPUT_TYPE_FLAG_EXECUTABLE,  NULL, 1, "__start_ and __end_ symbols");

    assert_sections(elf_file,
        // Name            Type            Address   Offset  Size     Flags                      Align
        ".text",           SHT_PROGBITS,   0x401000, 0x1000, 0x001a,  SHF_ALLOC | SHF_EXECINSTR, 16,
        "foo",             SHT_PROGBITS,   0x402000, 0x2000, 0x0004,  SHF_ALLOC | SHF_WRITE,     1,
        NULL
    );

    assert_program_segments(elf_file,
        // Type           Offset   VirtAddr   FileSiz  MemSiz  Flags         Align
        PT_LOAD,          0x0000,  0x400000,  0x180,   0x180,  PF_R,         0x1000,
        PT_LOAD,          0x1000,  0x401000,  0x001a,  0x001a, PF_R | PF_X,  0x1000,
        PT_LOAD,          0x2000,  0x402000,  0x0004,  0x0004, PF_R | PF_W,  0x1000,
        END
    );

    assert_symtab(elf_file,
    //  Value      Size   Type        Binding     Visibility   Section  Name
        0x401000,  0,     STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, ".text", "_start",
        0x402000,  0,     STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, "ABS",   "__start_foo",
        0x402004,  0,     STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, "ABS",   "__stop_foo",
        END);
}

// Test a library with no relocations, only two objects in .data and .bss. and two functions
static void test_shared_library_no_dependencies() {
    char *object_path = run_was(
        ".globl i;"
        ".globl j;"
        ".data;"
        "    i: .long 1;"
        "    j: .long 2;"
        ".comm k, 4, 4;"
        ".comm l, 4, 4;"
        ".globl f1;"
        ".globl f2;"
        ".text;"
        "    f1: nop;"
        "    f2: nop;"
    );

    List *input_paths = new_list(1);
    append_to_list(input_paths, object_path);

    char *lib_name;
    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_SHARED, &lib_name, 0, "test_shared_library_no_dependencies");

    assert_sections(elf_file,
        // Name            Type            Address   Offset  Size   Flags                      Align
        ".hash",           SHT_HASH,       0x1000,   0x1000, 0x2c,  SHF_ALLOC,                 8,
        ".dynsym",         SHT_DYNSYM,     0x102c,   0x102c, 0xa8,  SHF_ALLOC,                 1,
        ".dynstr",         SHT_STRTAB,     0x10d4,   0x10d4, 0x0f,  SHF_ALLOC,                 1,
        ".text",           SHT_PROGBITS,   0x2000,   0x2000, 0x02,  SHF_ALLOC | SHF_EXECINSTR, 16,
        ".dynamic",        SHT_DYNAMIC,    0x3000,   0x3000, 0x70,  SHF_ALLOC | SHF_WRITE,     8,
        ".data",           SHT_PROGBITS,   0x3070,   0x3070, 0x08,  SHF_ALLOC | SHF_WRITE,     4,
        ".bss",            SHT_NOBITS,     0x3078,   0x3078, 0x08,  SHF_ALLOC | SHF_WRITE,     4,
        NULL
    );

    assert_program_segments(elf_file,
        // Type           Offset   VirtAddr FileSiz  MemSiz  Flags         Align
        PT_LOAD,          0x0000,  0x0000,  0x2c0,   0x2c0,  PF_R,         0x1000,
        PT_LOAD,          0x1000,  0x1000,  0x00e3,  0x00e3, PF_R,         0x1000,
        PT_LOAD,          0x2000,  0x2000,  0x0002,  0x0002, PF_R | PF_X,  0x1000,
        PT_LOAD,          0x3000,  0x3000,  0x0078,  0x0080, PF_R | PF_W,  0x1000,
        PT_DYNAMIC,       0x3000,  0x3000,  0x0070,  0x0070, PF_R | PF_W,  0x0008,
        END
    );

    assert_dynamic(elf_file,
        DT_STRTAB, 0x10d4, NULL,
        DT_SYMTAB, 0x102c, NULL,
        DT_STRSZ,  0xf,    NULL,
        DT_SYMENT, 0x18,   NULL,
        DT_HASH,   0x1000, NULL,
        DT_DEBUG,  0,      NULL,
        DT_NULL,   0,      NULL
    );

    assert_dynsym(elf_file,
    //  Value      Size   Type        Binding     Visibility   Section    Name
        0x3070,    0,     STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, ".data",   "i",
        0x3074,    0,     STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, ".data",   "j",
        0x3078,    4,     STT_OBJECT, STB_GLOBAL, STV_DEFAULT, ".bss",    "k",
        0x307c,    4,     STT_OBJECT, STB_GLOBAL, STV_DEFAULT, ".bss",    "l",
        0x2000,    0,     STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, ".text",   "f1",
        0x2001,    0,     STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, ".text",   "f2",
        END);

    assert_symtab(elf_file,
    //  Value      Size   Type        Binding     Visibility   Section  Name
        0x3000,    0,     STT_OBJECT, STB_LOCAL,  STV_DEFAULT, ".dynamic", "_DYNAMIC",
        0x3070,    0,     STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, ".data",    "i",
        0x3074,    0,     STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, ".data",    "j",
        0x3078,    4,     STT_OBJECT, STB_GLOBAL, STV_DEFAULT, ".bss",     "k",
        0x307c,    4,     STT_OBJECT, STB_GLOBAL, STV_DEFAULT, ".bss",     "l",
        0x2000,    0,     STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, ".text",    "f1",
        0x2001,    0,     STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, ".text",    "f2",
        END);
}

static void test_two_shared_libs_with_data() {
    // Make a lib with two ints
    char *object_path = run_was(
        ".globl i;"
        ".globl j;"
        ".globl k;"
        ".data;"
        "    i: .long 1;"
        "    j: .long 2;"
        "    k: .long 3;" // This must not be included in the .dynsym since it resolves nothing
    );

    List *input_paths = new_list(1);
    append_to_list(input_paths, object_path);

    char *lib_name;
    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_SHARED, &lib_name, 0, "test_two_shared_libs_with_data");
    char *lib_filename = malloc(strlen(lib_name) + 16);
    sprintf(lib_filename, "lib%s.so", &lib_name[1]);

    // Make a second lib that uses the two ints from the first
    object_path = run_was(
        ".text;"
        "mov i@GOTPCREL(%rip), %eax;"
        "mov j@GOTPCREL(%rip), %eax;"
    );

    input_paths = new_list(1);
    append_to_list(input_paths, object_path);
    append_to_list(input_paths, lib_name);

    elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_SHARED, &lib_name, 0, "test_two_shared_libs_with_data");

    assert_sections(elf_file,
        // Name            Type            Address   Offset  Size   Flags                      Align
        ".hash",           SHT_HASH,       0x1000,   0x1000, 0x18,  SHF_ALLOC,                 8,
        ".dynsym",         SHT_DYNSYM,     0x1018,   0x1018, 0x48,  SHF_ALLOC,                 1,
        ".dynstr",         SHT_STRTAB,     0x1060,   0x1060, 0x15,  SHF_ALLOC,                 1,
        ".rela.dyn",       SHT_RELA,       0x1078,   0x1078, 0x30,  SHF_ALLOC,                 8,
        ".text",           SHT_PROGBITS,   0x2000,   0x2000, 0x0c,  SHF_ALLOC | SHF_EXECINSTR, 16,
        ".dynamic",        SHT_DYNAMIC,    0x3000,   0x3000, 0xb0,  SHF_ALLOC | SHF_WRITE,     8,
        ".got",            SHT_PROGBITS,   0x30b0,   0x30b0, 0x10,  SHF_ALLOC | SHF_WRITE,     8,
        NULL
    );

    assert_program_segments(elf_file,
        // Type           Offset   VirtAddr FileSiz  MemSiz  Flags         Align
        PT_LOAD,          0x0000,  0x0000,  0x2c0,   0x2c0,  PF_R,         0x1000,
        PT_LOAD,          0x1000,  0x1000,  0x00a8,  0x00a8, PF_R,         0x1000,
        PT_LOAD,          0x2000,  0x2000,  0x000c,  0x000c, PF_R | PF_X,  0x1000,
        PT_LOAD,          0x3000,  0x3000,  0x00c0,  0x00c0, PF_R | PF_W,  0x1000,
        PT_DYNAMIC,       0x3000,  0x3000,  0x00b0,  0x00b0, PF_R | PF_W,  0x0008,
        END
    );

    assert_dynamic(elf_file,
        DT_NEEDED,      0,          lib_filename,
        DT_STRTAB,      0x1060,     NULL,
        DT_SYMTAB,      0x1018,     NULL,
        DT_STRSZ,       0x15,       NULL,
        DT_SYMENT,      0x18,       NULL,
        DT_HASH,        0x1000,     NULL,
        DT_RELA,        0x1078,     NULL,
        DT_RELASZ,      0x30,       NULL,
        DT_RELAENT,     0x18,       NULL,
        DT_DEBUG,       0,         NULL,
        DT_NULL,        0,          NULL
    );

    assert_symtab(elf_file,
    //  Value   Size   Type        Binding     Visibility   Section    Name
        0x3000, 0,     STT_OBJECT, STB_LOCAL, STV_DEFAULT, ".dynamic", "_DYNAMIC",
        0x30b0, 0,     STT_OBJECT, STB_LOCAL, STV_DEFAULT, ".got",     "_GLOBAL_OFFSET_TABLE_",
        END);

    assert_dynsym(elf_file,
    //  Value  Size   Type        Binding     Visibility   Section  Name
        0,     0,     STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, "UND",   "i",
        0,     0,     STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, "UND",   "j",
        END);

    assert_rela_dyn_relocations(elf_file,
        // Tag             Dyn symtab index  Offset   Addend
        R_X86_64_GLOB_DAT, 1,                0x30b0,  0,
        R_X86_64_GLOB_DAT, 2,                0x30b8,  0,
        END);
}

static void test_two_shared_libs_with_functions() {
    // Make a lib with two functions
    char *object_path = run_was(
        ".globl f1;"
        ".globl f2;"
        ".text;"
        "    f1: nop;"
        "    f2: nop;"
    );

    List *input_paths = new_list(1);
    append_to_list(input_paths, object_path);

    char *lib_name;
    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_SHARED, &lib_name, 0, "test_two_shared_libs_with_functions");
    char *lib_filename = malloc(strlen(lib_name) + 16);
    sprintf(lib_filename, "lib%s.so", &lib_name[1]);

    // Make a second lib that uses the two functions from the first
    object_path = run_was(
        ".text;"
        "callq f1@PLT;"
        "callq f2@PLT;"
    );

    input_paths = new_list(1);
    append_to_list(input_paths, object_path);
    append_to_list(input_paths, lib_name);

    elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_SHARED, &lib_name, 0, "test_two_shared_libs_with_functions");

    assert_sections(elf_file,
        // Name            Type            Address   Offset  Size   Flags                      Align
        ".hash",           SHT_HASH,       0x1000,   0x1000, 0x18,  SHF_ALLOC,                 8,
        ".dynsym",         SHT_DYNSYM,     0x1018,   0x1018, 0x48,  SHF_ALLOC,                 1,
        ".dynstr",         SHT_STRTAB,     0x1060,   0x1060, 0x17,  SHF_ALLOC,                 1,
        ".rela.plt",       SHT_RELA,       0x2000,   0x2000, 0x30,  SHF_ALLOC | SHF_INFO_LINK, 8,
        ".plt",            SHT_PROGBITS,   0x3000,   0x3000, 0x30,  SHF_ALLOC | SHF_EXECINSTR, 8,
        ".text",           SHT_PROGBITS,   0x3030,   0x3030, 0x0a,  SHF_ALLOC | SHF_EXECINSTR, 16,
        ".dynamic",        SHT_DYNAMIC,    0x4000,   0x4000, 0xc0,  SHF_ALLOC | SHF_WRITE,     8,
        ".got.plt",        SHT_PROGBITS,   0x40c0,   0x40c0, 0x28,  SHF_ALLOC | SHF_WRITE,     8,
        NULL
    );

    assert_program_segments(elf_file,
        // Type           Offset   VirtAddr FileSiz  MemSiz  Flags         Align
        PT_LOAD,          0x0000,  0x0000,  0x0300,  0x0300, PF_R,         0x1000,
        PT_LOAD,          0x1000,  0x1000,  0x0077,  0x0077, PF_R,         0x1000,
        PT_LOAD,          0x2000,  0x2000,  0x0030,  0x0030, PF_R,         0x1000,
        PT_LOAD,          0x3000,  0x3000,  0x003a,  0x003a, PF_R | PF_X,  0x1000,
        PT_LOAD,          0x4000,  0x4000,  0x00e8,  0x00e8, PF_R | PF_W,  0x1000,
        PT_DYNAMIC,       0x4000,  0x4000,  0x00c0,  0x00c0, PF_R | PF_W,  0x0008,
        END
    );

    assert_dynamic(elf_file,
        DT_NEEDED,      0,          lib_filename,
        DT_STRTAB,      0x1060,     NULL,
        DT_SYMTAB,      0x1018,     NULL,
        DT_STRSZ,       0x17,       NULL,
        DT_SYMENT,      0x18,       NULL,
        DT_HASH,        0x1000,     NULL,
        DT_PLTGOT,      0x40c0,     NULL,
        DT_PLTRELSZ,    0x30,       NULL,
        DT_PLTREL,      0x07,       NULL,
        DT_JMPREL,      0x2000,     NULL,
        DT_DEBUG,       0,          NULL,
        DT_NULL,        0,          NULL
    );

    assert_symtab(elf_file,
    //  Value   Size   Type        Binding     Visibility   Section    Name
        0x4000, 0,     STT_OBJECT, STB_LOCAL, STV_DEFAULT, ".dynamic", "_DYNAMIC",
        0x40c0, 0,     STT_OBJECT, STB_LOCAL, STV_DEFAULT, ".got.plt", "_GLOBAL_OFFSET_TABLE_",
        END);

    assert_dynsym(elf_file,
    //  Value  Size   Type        Binding     Visibility   Section  Name
        0,     0,     STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, "UND",   "f1",
        0,     0,     STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, "UND",   "f2",
        END);

    assert_rela_plt_relocations(elf_file,
        // Tag              Dyn symtab index  Offset   Addend
        R_X86_64_JUMP_SLOT, 1,                0x40d8,  0,
        R_X86_64_JUMP_SLOT, 2,                0x40e0,  0,
        END);

    // .rela.plt must have 3 entries. A The plt0 code + code for f1 and f2.
    assert_section_data(elf_file, PLT_SECTION_NAME,

        // PLT entry 0
        0xff, 0x35, 0xc2, 0x10, 0x00, 0x00,     // 0xff, 0x35, 0xb2, 0x10, 0x00, 0x00, // pushq .got.plt+8(%rip)
        0xff, 0x25, 0xc4, 0x10, 0x00, 0x00,     // 0xff, 0x25, 0xb4, 0x10, 0x00, 0x00, // jmpq .got.plt+16(%rip)
        0x0f, 0x1f, 0x40, 0x00,                 // 0x0f, 0x1f, 0x40, 0x00,             // nopl 0x0(%rax)

        // PLT entry 1
        0xff, 0x25, 0xc2, 0x10, 0x00, 0x00,     // jmpq *.got.plt+...(%rip)
        0x68, 0x00, 0x00, 0x00, 0x00,           // pushq $rela_dyn_index
        0xe9, 0xe0, 0xff, 0xff, 0xff,           // jmpq .plt0

        // PLT entry 2
        0xff, 0x25, 0xba, 0x10, 0x00, 0x00,
        0x68, 0x01, 0x00, 0x00, 0x00,
        0xe9, 0xd0, 0xff, 0xff, 0xff,

        END);

    // .got.plt must have 5 entries
    InputSection *got_plt = get_extra_section(elf_file, GOT_PLT_SECTION_NAME);
    if (!got_plt) panic("Could not find .got.plt");
    uint64_t *got_plt_data = got_plt->data;

    assert_uint64_t(0x4000, got_plt_data[0], ".got.plt[0]"); // The address of the .dynamic section
    assert_uint64_t(0x0000, got_plt_data[1], ".got.plt[1]"); // Used by program linker, must be zero
    assert_uint64_t(0x0000, got_plt_data[2], ".got.plt[2]"); // Used by program linker, must be zero
    assert_uint64_t(0x3016, got_plt_data[3], ".got.plt[3]"); // .plt1 + 6, the address of the pushq instruction
    assert_uint64_t(0x3026, got_plt_data[4], ".got.plt[4]"); // .plt2 + 6
}

static void test_shared_lib_with_two_objects() {
    char *object_path1 = run_was(
        ".text;"

        "_start:;"
        "    mov i@GOTPCREL(%rip), %eax;" // Produces a relocation in .rela.dyn
        "    callq f@PLT;"                // Produces a relocation in .rela.plt
    );

    char *object_path2 = run_was(
        ".text;"
        ".globl f;"
        ".globl i;"

        "f:;"
        "    nop;"
        ".section .data;"
        "    i: .long 0"
    );

    List *input_paths = new_list(1);
    append_to_list(input_paths, object_path1);
    append_to_list(input_paths, object_path2);

    char *lib_name;
    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_SHARED, &lib_name, 0, "shared_lib_with_two_objects");
    char *lib_filename = malloc(strlen(lib_name) + 16);
    sprintf(lib_filename, "lib%s.so", &lib_name[1]);

    // The relocations must be resolved my the dynamic loader, since it's a shared library,
    // meaning the i and f symbols could be overridden. Therefore, they cannot be
    // linkes statically.

    // Relocation for i
    assert_rela_dyn_relocations(elf_file,
        // Tag             Dyn symtab index  Offset   Addend
        R_X86_64_GLOB_DAT, 2,                0x40e0,  0,
        END);

    // Relocation for f
    assert_rela_plt_relocations(elf_file,
        // Tag              Dyn symtab index  Offset   Addend
        R_X86_64_JUMP_SLOT, 1,                0x4100,  0,
        END);

    assert_dynsym(elf_file,
    //  Value      Size   Type        Binding     Visibility   Section    Name
        0x3030,    0,     STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, ".text",   "f",
        0x4108,    0,     STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, ".data",   "i",
        END);
}

static void test_dynamic_executable_with_GOT_entry_for_local_symbol() {
    // Run with -mrelax-relocations=no to get a GOTPCREL relocation instead of the default GOTPCRELX.
    char *object_path = run_as("-mrelax-relocations=no",
        ".type _start, @function;"
        ".globl _start;"
        ".section .text;"
        "_start:;"
        "    movq foo@GOTPCREL(%rip), %rax;"
        ".section .data;"
        "    foo: .quad 100;"

    );

    List *input_paths = new_list(1);
    append_to_list(input_paths, object_path);

    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_SHARED | OUTPUT_TYPE_FLAG_EXECUTABLE, NULL, 0, "dynamic_executable_with_GOT_entry_for_local_symbol");

    assert_sections(elf_file,
        // Name            Type            Address   Offset  Size   Flags                      Align
        ".hash",           SHT_HASH,       0x1000,   0x1000, 0x10,  SHF_ALLOC,                 8,
        ".dynsym",         SHT_DYNSYM,     0x1010,   0x1010, 0x18,  SHF_ALLOC,                 1,
        ".dynstr",         SHT_STRTAB,     0x1028,   0x1028, 0x01,  SHF_ALLOC,                 1,
        ".rela.dyn",       SHT_RELA,       0x1030,   0x1030, 0x18,  SHF_ALLOC,                 8,
        ".text",           SHT_PROGBITS,   0x2000,   0x2000, 0x07,  SHF_ALLOC | SHF_EXECINSTR, 1,
        ".dynamic",        SHT_DYNAMIC,    0x3000,   0x3000, 0xa0,  SHF_ALLOC | SHF_WRITE,     8,
        ".got",            SHT_PROGBITS,   0x30a0,   0x30a0, 0x08,  SHF_ALLOC | SHF_WRITE,     8,
        ".data",           SHT_PROGBITS,   0x30a8,   0x30a8, 0x08,  SHF_ALLOC | SHF_WRITE,     1,
        NULL
    );

    // Relocation for foo
    assert_rela_dyn_relocations(elf_file,
        // Tag             Dyn symtab index  Offset   Addend
        R_X86_64_RELATIVE, 0,                0x30a0,  0x30a8,
        END);

    // Check foo is in the symtab, but not the dynsym
    assert_dynsym(elf_file, END);

    assert_symtab(elf_file,
    //  Value      Size   Type        Binding     Visibility   Section     Name
        0x3000,    0,     STT_OBJECT, STB_LOCAL,  STV_DEFAULT, ".dynamic", "_DYNAMIC",
        0x30a0,    0,     STT_OBJECT, STB_LOCAL,  STV_DEFAULT, ".got",     "_GLOBAL_OFFSET_TABLE_",
        0x30a8,    0,     STT_NOTYPE, STB_LOCAL,  STV_DEFAULT, ".data",    "foo",
        0x2000,    0,     STT_FUNC,   STB_GLOBAL, STV_DEFAULT, ".text",    "_start",
        END);
}

void test_dynamic_executable_with_GOT_entry_for_local_symbol_and_GOT_relocation_too() {
    // Make an dynamic executable with two R_X86_64_REX_GOTPCRELX relocations,
    // one in a mov
    // and another in a cmpq
    char *object_path = run_was(
        ".section .data;"
        "d: .quad 100;"
        ".section .text;"
        ".globl _start;"
        "_start:;"
        // Gets rewritten to a lea using a PC-relavtive address to d
        "    movq d@GOTPCREL(%rip), %rax;"
        // Not rewritten, but needs the GOT entry added to the executable
        "    cmpq d@GOTPCREL(%rip), %rax;"
    );

    List *input_paths = new_list(1);
    append_to_list(input_paths, object_path);

    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_SHARED | OUTPUT_TYPE_FLAG_EXECUTABLE, NULL, 0, "dynamic_executable_with_GOT_entry_for_local_symbol_and_GOT_relocation_too");

    assert_sections(elf_file,
        // Name            Type            Address   Offset  Size   Flags                      Align
        ".hash",           SHT_HASH,       0x1000,   0x1000, 0x10,  SHF_ALLOC,                 8,
        ".dynsym",         SHT_DYNSYM,     0x1010,   0x1010, 0x18,  SHF_ALLOC,                 1,
        ".dynstr",         SHT_STRTAB,     0x1028,   0x1028, 0x01,  SHF_ALLOC,                 1,
        ".rela.dyn",       SHT_RELA,       0x1030,   0x1030, 0x18,  SHF_ALLOC,                 8,
        ".text",           SHT_PROGBITS,   0x2000,   0x2000, 0x0e,  SHF_ALLOC | SHF_EXECINSTR, 16,
        ".dynamic",        SHT_DYNAMIC,    0x3000,   0x3000, 0xa0,  SHF_ALLOC | SHF_WRITE,     8,
        ".got",            SHT_PROGBITS,   0x30a0,   0x30a0, 0x08,  SHF_ALLOC | SHF_WRITE,     8,
        ".data",           SHT_PROGBITS,   0x30a8,   0x30a8, 0x08,  SHF_ALLOC | SHF_WRITE,     4,
        NULL
    );

    // Relocation for foo
    assert_rela_dyn_relocations(elf_file,
        // Tag             Dyn symtab index  Offset   Addend
        R_X86_64_RELATIVE, 0,                0x30a0,  0x30a8,
        END);

    // .got is at 0x3090
    // .text stats at 0x2000
    // The displacement is calculated from the next instruction. The instruction lengths are 7.
    assert_section_data(elf_file, ".text",
        0x48, 0x8d, 0x05, 0xa1, 0x10, 0x00, 0x00, // lea 0x1091(%rip),%rax 0x30a0 - 0x2007 = 0x10a1
        0x48, 0x3b, 0x05, 0x92, 0x10, 0x00, 0x00, // cmp 0x1082(%rip),%rax 0x30a0 - 0x200e = 0x1092
        END);
}

// Ensure the relocation to .debug_abbrev is correct and the address is zero.
// Ensure the relocation to .text is set in .debug_lines.
// This tests both static and dynamic executables.
static void test_dwarf() {
    char *object_path = run_was(
        ".globl _start;"
        ".text;"
        "_start:;"
        "   .file 1 \"test.c\";"
        "   .loc 1 4;"
        "    movl $1, %eax;"
        "   .loc 1 4;"
        "    movl $0, %ebx;"
        "    int $0x80;"
        ".section foo, \"aw\", @progbits;"
        "   .long 1;"
        // Incomplete debug info
        ".section .debug_info, \"\", @progbits;\n"
        "   .long .debug_abbrev;"
        ".section .debug_abbrev, \"\", @progbits;\n"
        "   .long 0;"
    );

    List *input_paths = new_list(1);
    append_to_list(input_paths, object_path);

    // Static executable
    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_STATIC | OUTPUT_TYPE_FLAG_EXECUTABLE,  NULL, 1, "dwarf lines");

    assert_sections(elf_file,
        // Name           Type            Address   Offset  Size   Flags                      Align
        ".text",          SHT_PROGBITS,   0x401000, 0x1000, 0x0c,  SHF_ALLOC | SHF_EXECINSTR, 16,
        "foo",            SHT_PROGBITS,   0x402000, 0x2000, 0x04,  SHF_ALLOC | SHF_WRITE,     1,
        ".debug_info",    SHT_PROGBITS,   0x000000, 0x3000, 0x04,  0,                         1,
        ".debug_abbrev",  SHT_PROGBITS,   0x000000, 0x3004, 0x04,  0,                         1,
        ".debug_line",    SHT_PROGBITS,   0x000000, 0x3008, 0x38,  0,                         0,
        NULL
    );

    // Ensure that the .debug_abbrev has address zero. This is required by DWARF.
    // No alloc sections must have address zero.
    OutputSection *debug_abbrev_section = get_output_section(elf_file, ".debug_abbrev");
    if (!debug_abbrev_section) panic("Expected a .debug_abbrev section");
    assert_int(0, debug_abbrev_section->address, ".debug_abbrev has zero address");

    // Ensure the .debug_abbrev pointer in .debug_info points to the zero address.
    OutputSection *debug_info_section = get_output_section(elf_file, ".debug_info");
    assert_int(4, debug_info_section->size, ".debug_info size is 4");
    uint32_t *offset = (uint32_t *) debug_info_section->data;
    assert_int(0, *offset, ".debug_info pointer inside .debug_abbrev is zero");

    // The first instruction in the line number program is extended opcode 2 Set Address.
    // The address is at 0x2a. This is set by a R_X86_64_64 relocation that points at .text.
    OutputSection *debug_line = get_output_section(elf_file, ".debug_line");
    if (!debug_line) panic("Expected a .debug_line section");
    uint64_t address = (*(uint64_t *) (&debug_line->data[0x2a]));
    assert_uint64_t(0x401000, address, "Line number address is set to .text");

    // Dynamic executable
    elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_EXECUTABLE,  NULL, 1, "dwarf lines");

    assert_sections(elf_file,
        // Name           Type            Address   Offset  Size   Flags                      Align
        ".hash",          SHT_HASH,       0x1000,   0x1000, 0x10,  SHF_ALLOC,                 8,
        ".dynsym",        SHT_DYNSYM,     0x1010,   0x1010, 0x18,  SHF_ALLOC,                 1,
        ".dynstr",        SHT_STRTAB,     0x1028,   0x1028, 0x01,  SHF_ALLOC,                 1,
        ".text",          SHT_PROGBITS,   0x2000,   0x2000, 0x0c,  SHF_ALLOC | SHF_EXECINSTR, 16,
        ".dynamic",       SHT_DYNAMIC,    0x3000,   0x3000, 0x70,  SHF_ALLOC | SHF_WRITE,     8,
        "foo",            SHT_PROGBITS,   0x3070,   0x3070, 0x04,  SHF_ALLOC | SHF_WRITE,     1,
        ".debug_info",    SHT_PROGBITS,   0x0000,   0x4000, 0x04,  0,                         1,
        ".debug_abbrev",  SHT_PROGBITS,   0x0000,   0x4004, 0x04,  0,                         1,
        ".debug_line",    SHT_PROGBITS,   0x0000,   0x4008, 0x38,  0,                         0,
        NULL
    );

    // The first instruction in the line number program is extended opcode 2 Set Address.
    // The address is at 0x2a. This is set by a R_X86_64_64 relocation that points at .text.
    debug_line = get_output_section(elf_file, ".debug_line");
    if (!debug_line) panic("Expected a .debug_line section");
    address = (*(uint64_t *) (&debug_line->data[0x2a]));
    assert_uint64_t(0x2000, address, "Line number address is set to .text");
}

// Test statically linking an executable to an archive file via a GNU ld script
static void test_gnu_ld_script_archive() {
    char *object_path1 = run_was(
        ".text;"
        ".globl _start;"

        "_start:;"
        "    movl $1, %eax;"
        "    lea code(%rip), %rdi;"
        "    mov (%rdi), %ebx;"
        "    subq $42, %ebx;"
        "    int $0x80;"
    );

    char *object_path2 = run_was(
        ".globl code;"
        ".data;"
        "    code: .long 42;"
    );

    // Make an library file from the second object file
    char command[1024];
    sprintf(command, "ar rcs /tmp/test.a %s\n", object_path2);
    int result = system(command);
    if (result) {
        printf("Ar failed with exit code %d\n", result >> 8);
        exit(1);
    }

    // Write a GNU script referencing the library
    write_file("/tmp/libgnuldscript.a",
        "/* GNU ld script\n"
        "*/\n"
        "OUTPUT_FORMAT(elf64-x86-64)\n"
        "GROUP(/tmp/test.a)\n"
    );

    List *input_paths = new_list(1);
    append_to_list(input_paths, object_path1);
    append_to_list(input_paths, "*gnuldscript");

    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_STATIC | OUTPUT_TYPE_FLAG_EXECUTABLE,  NULL, 1, "linking to a GNU ld script archive");

    assert_sections(elf_file,
        // Name           Type            Address   Offset  Size   Flags                      Align
        ".text",          SHT_PROGBITS,   0x401000, 0x1000, 0x20,  SHF_ALLOC | SHF_EXECINSTR, 16,
        ".data",          SHT_PROGBITS,   0x402000, 0x2000, 0x04,  SHF_ALLOC | SHF_WRITE,     4,
        NULL
    );
}

// There is not much here other than the program segments check
static void test_dynamic_executable_sanity() {
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

    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_SHARED | OUTPUT_TYPE_FLAG_EXECUTABLE, NULL, 1, "test_dynamic_executable_sanity");

    assert_program_segments(elf_file,
        // Type           Offset   VirtAddr   FileSiz  MemSiz  Flags         Align
        PT_PHDR,          0x0040,  0x000040,  0x150,   0x150,  PF_R,         0x0008,
        PT_LOAD,          0x0000,  0x00000,   0x280,   0x280,  PF_R,         0x1000,
        PT_LOAD,          0x1000,  0x001000,  0x029,   0x29,   PF_R,         0x1000,
        PT_LOAD,          0x2000,  0x002000,  0x010,   0x10,   PF_R | PF_X,  0x1000,
        PT_LOAD,          0x3000,  0x003000,  0x074,   0x74,   PF_R | PF_W,  0x1000,
        PT_DYNAMIC,       0x3000,  0x003000,  0x070,   0x70,   PF_R | PF_W,  0x0008,
        END
    );

    assert_symtab(elf_file,
    //  Value      Size   Type        Binding     Visibility   Section     Name
        0x3000,    0,     STT_OBJECT, STB_LOCAL,  STV_DEFAULT, ".dynamic", "_DYNAMIC",
        0x3070,    0,     STT_NOTYPE, STB_LOCAL,  STV_DEFAULT, ".data",    "code",
        0x2000,    0,     STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, ".text",    "_start",
        END);

    assert_dynsym(elf_file, END);
}

void test_dynamic_executable_using_an_object_in_a_library() {
    char *library_path = run_was(
        ".globl i;"
        ".globl j;"
        ".type i, @object;" // A char
        ".size i, 1;"       //
        ".type j, @object;" // An int
        ".size j, 4;"       //
        ".data;"
        "    i: .byte 1;"
        ".align 4;"         // Align to 4 bytes
        "    j: .long 2;"
    );

    List *input_paths = new_list(1);
    append_to_list(input_paths, library_path);

    char *lib_name;
    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_SHARED, &lib_name, 0, "dynamic_executable_using_an_object_in_a_library");

    char *object_path = run_was(
        ".globl _start;"
        ".text;"
        "_start:;"
        "    movb i(%rip), %al;"
        "    movl j(%rip), %eax;"
    );

    input_paths = new_list(1);
    append_to_list(input_paths, object_path);
    append_to_list(input_paths, lib_name);

    elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_SHARED | OUTPUT_TYPE_FLAG_EXECUTABLE, NULL, 0, "dynamic_executable_using_an_object_in_a_library");

    assert_sections(elf_file,
        // Name           Type            Address   Offset  Size   Flags                      Align
        ".hash",           SHT_HASH,       0x1000,   0x1000, 0x18,  SHF_ALLOC,                 8,
        ".dynsym",         SHT_DYNSYM,     0x1018,   0x1018, 0x48,  SHF_ALLOC,                 1,
        ".dynstr",         SHT_STRTAB,     0x1060,   0x1060, 0x15,  SHF_ALLOC,                 1,
        ".rela.dyn",       SHT_RELA,       0x1078,   0x1078, 0x30,  SHF_ALLOC,                 8,
        ".text",           SHT_PROGBITS,   0x2000,   0x2000, 0x0c,  SHF_ALLOC | SHF_EXECINSTR, 16,
        ".dynamic",        SHT_DYNAMIC,    0x3000,   0x3000, 0xb0,  SHF_ALLOC | SHF_WRITE,     8,
        ".data",           SHT_PROGBITS,   0x30b0,   0x30b0, 0x08,  SHF_ALLOC | SHF_WRITE,     4,
        NULL
    );

    assert_dynsym(elf_file,
    //  Value      Size   Type        Binding     Visibility   Section    Name
        0x30b0,    1,     STT_OBJECT, STB_GLOBAL, STV_DEFAULT, ".data",   "i",
        0x30b4,    4,     STT_OBJECT, STB_GLOBAL, STV_DEFAULT, ".data",   "j",
        END);

    assert_rela_dyn_relocations(elf_file,
        // Tag             Dyn symtab index  Offset   Addend
        R_X86_64_COPY,     1,                0x30b0,  0,
        R_X86_64_COPY,     2,                0x30b4,  0,
        END);
}

// Test the conversion of a R_X86_64_64 to a R_X86_64_RELATIVE in dynamic executables
// This tests a relocation from the .rodata section, without there being a symbol
void test_dynamic_executable_R_X86_64_RELATIVE_relocations_from_rodata() {
    char *object_path = run_was(
        ".globl foo;"
        ".globl _start;"
        ".data;"
        "    .align   8;"
        "    .type    foo, @object;"
        "    .size    foo, 8;"
        "    foo: .quad .SL0;;"
        ".section .rodata;"
        "   .SL0: .string \"Hello World!\";"
        ".text;"
        "_start: nop;"
    );

    List *input_paths = new_list(1);
    append_to_list(input_paths, object_path);

    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_SHARED | OUTPUT_TYPE_FLAG_EXECUTABLE, NULL, 0, "dynamic_executable_R_X86_64_RELATIVE_relocations");

    assert_sections(elf_file,
        // Name            Type            Address   Offset  Size   Flags                      Align
        ".hash",           SHT_HASH,       0x1000,   0x1000, 0x10,  SHF_ALLOC,                 8,
        ".dynsym",         SHT_DYNSYM,     0x1010,   0x1010, 0x18,  SHF_ALLOC,                 1,
        ".dynstr",         SHT_STRTAB,     0x1028,   0x1028, 0x01,  SHF_ALLOC,                 1,
        ".rela.dyn",       SHT_RELA,       0x1030,   0x1030, 0x18,  SHF_ALLOC,                 8,
        ".text",           SHT_PROGBITS,   0x2000,   0x2000, 0x01,  SHF_ALLOC | SHF_EXECINSTR, 16,
        ".rodata",         SHT_PROGBITS,   0x3000,   0x3000, 0x0d,  SHF_ALLOC,                 4,
        ".dynamic",        SHT_DYNAMIC,    0x4000,   0x4000, 0xa0,  SHF_ALLOC | SHF_WRITE,     8,
        ".data",           SHT_PROGBITS,   0x40a0,   0x40a0, 0x08,  SHF_ALLOC | SHF_WRITE,     4,
        NULL
    );

    assert_dynsym(elf_file, END);

    assert_rela_dyn_relocations(elf_file,
        // Tag             Dyn symtab index  Offset   Addend
        R_X86_64_RELATIVE, 0,                0x40a0,  0x3000,
        END);
}

// Test the conversion of a R_X86_64_64 to a R_X86_64_RELATIVE in dynamic executables
// This tests a relocation from initialized data
void test_dynamic_executable_R_X86_64_RELATIVE_relocations_from_initialized_data() {
    char *object_path = run_was(
        ".globl _start;"
        ".globl x;"
        ".globl px;"
        ".data;"
        "    .align   4;"
        "    .type    x, @object;"
        "    .size    x, 4;"
        "x:  .long    42;"
        "    .align   8;"
        "    .type    px, @object;"
        "    .size    px, 8;"
        "px:;"
        "    .quad    x;"
        ".text;"
        "_start: nop;"
    );

    List *input_paths = new_list(1);
    append_to_list(input_paths, object_path);

    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_SHARED | OUTPUT_TYPE_FLAG_EXECUTABLE, NULL, 0, "dynamic_executable_R_X86_64_RELATIVE_relocations");

    assert_dynsym(elf_file, END);

    assert_rela_dyn_relocations(elf_file,
        // Tag             Dyn symtab index  Offset   Addend
        R_X86_64_RELATIVE, 0,                0x30a8,  0x30a0,
        END);
}

// Test the conversion of a R_X86_64_64 to a R_X86_64_RELATIVE in dynamic executables
// This tests a relocation from uninitialized data
void test_dynamic_executable_R_X86_64_RELATIVE_relocations_from_uninitialized_data() {
    char *object_path = run_was(
        ".globl _start;"
        ".text;"
        ".comm x,4,4;"
        ".globl px;"
        ".data;"
        "   .align   8;"
        "   .type    px, @object;"
        "   .size    px, 8;"
        "px: .quad x;"
        ".text;"
        "_start: nop;"
    );

    List *input_paths = new_list(1);
    append_to_list(input_paths, object_path);

    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_SHARED | OUTPUT_TYPE_FLAG_EXECUTABLE, NULL, 0, "dynamic_executable_R_X86_64_RELATIVE_relocations");

    assert_dynsym(elf_file, END);

    assert_rela_dyn_relocations(elf_file,
        // Tag             Dyn symtab index  Offset   Addend
        R_X86_64_RELATIVE, 0,                0x30a0,  0x30a8,
        END);
}

// This tests the offset and addend handling in the relocation code
void test_dynamic_executable_R_X86_64_RELATIVE_relocations_from_two_files() {
    char *object_path1 = run_was(
        "    .globl   foo;"
        "    .data;"
        "    .align   8;"
        "    .type    foo, @object;"
        "    .size    foo, 8;"
        "foo: .quad .SL0;"
        ".section .rodata;"
        ".SL0: .string \"foo\";"
        ".globl _start;"
        ".text;"
        "_start: nop;"
    );

    char *object_path2 = run_was(
        "    .globl   bar;"
        "    .data;"
        "    .align   8;"
        "    .type    bar, @object;"
        "    .size    bar, 8;"
        "bar: .quad .SL0;"
        ".section .rodata;"
        ".SL0: .string \"bar\";"
    );

    List *input_paths = new_list(1);
    append_to_list(input_paths, object_path1);
    append_to_list(input_paths, object_path2);

    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_SHARED | OUTPUT_TYPE_FLAG_EXECUTABLE, NULL, 0, "dynamic_executable_R_X86_64_RELATIVE_relocations");

    assert_dynsym(elf_file, END);

    assert_rela_dyn_relocations(elf_file,
        // Tag             Dyn symtab index  Offset   Addend
        R_X86_64_RELATIVE, 0,                0x40a0,  0x3000,
        R_X86_64_RELATIVE, 0,                0x40a8,  0x3004,
        END);
}

// An executable calls a function in a shared library through a function pointer
// The function pointer has a R_X86_64_64 relocation which must make it
// into the .rela.dyn of the executable.
void test_dynamic_executable_R_X86_64_64_relocation() {
    // Make a lib with a function
    char *object_path1 = run_was(
        ".globl f;"
        ".text;"
        "f:"
        "    movl $42, %eax;"
        "    ret;"
    );

    List *input_paths = new_list(1);
    append_to_list(input_paths, object_path1);

    char *lib_name;
    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_SHARED, &lib_name, 0, "dynamic_executable_R_X86_64_64_relocation");
    char *lib_filename = malloc(strlen(lib_name) + 16);
    sprintf(lib_filename, "lib%s.so", &lib_name[1]);

    char *object_path2 = run_was(
    "    .data;"
    "    .align   8;"
    "    .type    pf, @object;"
    "    .size    pf, 8;"
    "pf:;"
    "    .quad    f;"
    "    .text;"
    "    .globl  _start;"
    "_start:;"
    "    movq        pf(%rip), %rbx;"
    "    callq       *%rbx;"
    "    movl        $1, %eax;"
    "    movl        $0, %ebx;" // Exit code
    "    int        $0x80;"
    );

    input_paths = new_list(1);
    append_to_list(input_paths, object_path2);
    append_to_list(input_paths, lib_name);

    elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_SHARED | OUTPUT_TYPE_FLAG_EXECUTABLE, NULL, 0, "dynamic_executable_R_X86_64_64_relocation");

    assert_sections(elf_file,
        // Name            Type            Address   Offset  Size   Flags                      Align
        ".hash",           SHT_HASH,       0x1000,   0x1000, 0x14,  SHF_ALLOC,                 8,
        ".dynsym",         SHT_DYNSYM,     0x1014,   0x1014, 0x30,  SHF_ALLOC,                 1,
        ".dynstr",         SHT_STRTAB,     0x1044,   0x1044, 0x13,  SHF_ALLOC,                 1,
        ".rela.dyn",       SHT_RELA,       0x1058,   0x1058, 0x18,  SHF_ALLOC,                 8,
        ".text",           SHT_PROGBITS,   0x2000,   0x2000, 0x15,  SHF_ALLOC | SHF_EXECINSTR, 16,
        ".dynamic",        SHT_DYNAMIC,    0x3000,   0x3000, 0xb0,  SHF_ALLOC | SHF_WRITE,     8,
        ".data",           SHT_PROGBITS,   0x30b0,   0x30b0, 0x08,  SHF_ALLOC | SHF_WRITE,     4,
        NULL
    );

    assert_dynsym(elf_file,
    //  Value      Size   Type        Binding     Visibility   Section    Name
        0x0000,    0,     STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, "UND",     "f",
        END);

    assert_rela_dyn_relocations(elf_file,
        // Tag             Dyn symtab index  Offset   Addend
        R_X86_64_64,       1,                0x30b0,  0x0000,
        END);
}

// Similar to test_dynamic_executable_R_X86_64_64_relocation,
// except that it's a library a function in another library through a pointer
void test_dynamic_library_R_X86_64_64_relocation_for_function_pointer() {
    // Make a lib with a function
    char *object_path1 = run_was(
        ".globl f;"
        ".text;"
        "f:"
        "    movl $42, %eax;"
        "    ret;"
    );

    List *input_paths = new_list(1);
    append_to_list(input_paths, object_path1);

    char *lib_name;
    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_SHARED, &lib_name, 0, "dynamic_library_R_X86_64_64_relocation");
    char *lib_filename = malloc(strlen(lib_name) + 16);
    sprintf(lib_filename, "lib%s.so", &lib_name[1]);

    char *object_path2 = run_was(
    "    .data;"
    "    .align   8;"
    "    .type    pf, @object;"
    "    .size    pf, 8;"
    "pf:;"
    "    .quad    f;"
    "    .text;"
    "    .globl  _start;"
    "g:;"
    "    movq        pf(%rip), %rbx;"
    "    callq       *%rbx;"
    );

    input_paths = new_list(1);
    append_to_list(input_paths, object_path2);
    append_to_list(input_paths, lib_name);

    elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_SHARED, &lib_name, 0, "dynamic_library_R_X86_64_64_relocation");

    assert_sections(elf_file,
        // Name            Type            Address   Offset  Size   Flags                      Align
        ".hash",           SHT_HASH,       0x1000,   0x1000, 0x14,  SHF_ALLOC,                 8,
        ".dynsym",         SHT_DYNSYM,     0x1014,   0x1014, 0x30,  SHF_ALLOC,                 1,
        ".dynstr",         SHT_STRTAB,     0x1044,   0x1044, 0x13,  SHF_ALLOC,                 1,
        ".rela.dyn",       SHT_RELA,       0x1058,   0x1058, 0x18,  SHF_ALLOC,                 8,
        ".text",           SHT_PROGBITS,   0x2000,   0x2000, 0x09,  SHF_ALLOC | SHF_EXECINSTR, 16,
        ".dynamic",        SHT_DYNAMIC,    0x3000,   0x3000, 0xb0,  SHF_ALLOC | SHF_WRITE,     8,
        ".data",           SHT_PROGBITS,   0x30b0,   0x30b0, 0x08,  SHF_ALLOC | SHF_WRITE,     4,
        NULL
    );

    assert_dynsym(elf_file,
    //  Value      Size   Type        Binding     Visibility   Section    Name
        0x0000,    0,     STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, "UND",     "f",
        END);

    assert_rela_dyn_relocations(elf_file,
        // Tag             Dyn symtab index  Offset   Addend
        R_X86_64_64,       1,                0x30b0,  0x0000,
        END);
}


void test_dynamic_library_R_X86_64_64_relocation_when_making_a_shared_library() {
    // Make a lib with some data and a pointer to it
    char *object_path = run_was(
        "	.data;"
        ".globl d;"
        "	.type	d, @object;"
        "d:;"
        "	.long	42;"
        "	.globl	pd;"
        "	.type	pd, @object;"
        "pd:;"
        "	.quad	d;"             // Creates a R_X86_64_64_relocation
    );

    List *input_paths = new_list(1);
    append_to_list(input_paths, object_path);

    char *lib_name;
    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_SHARED, &lib_name, 0, "dynamic_library_R_X86_64_64_relocation_when_making_a_shared_library");

    assert_sections(elf_file,
        // Name            Type            Address   Offset  Size   Flags                      Align
        ".hash",           SHT_HASH,       0x1000,   0x1000, 0x18,  SHF_ALLOC,                 8,
        ".dynsym",         SHT_DYNSYM,     0x1018,   0x1018, 0x48,  SHF_ALLOC,                 1,
        ".dynstr",         SHT_STRTAB,     0x1060,   0x1060, 0x06,  SHF_ALLOC,                 1,
        ".rela.dyn",       SHT_RELA,       0x1068,   0x1068, 0x18,  SHF_ALLOC,                 8,
        ".dynamic",        SHT_DYNAMIC,    0x2000,   0x2000, 0xa0,  SHF_ALLOC | SHF_WRITE,     8,
        ".data",           SHT_PROGBITS,   0x20a0,   0x20a0, 0x0c,  SHF_ALLOC | SHF_WRITE,     4,
        NULL
    );

    assert_dynsym(elf_file,
    //  Value      Size   Type        Binding     Visibility   Section    Name
        0x20a0,    0,     STT_OBJECT, STB_GLOBAL, STV_DEFAULT, ".data",   "d",
        0x20a4,    0,     STT_OBJECT, STB_GLOBAL, STV_DEFAULT, ".data",   "pd",
        END);

    assert_rela_dyn_relocations(elf_file,
        // Tag             Dyn symtab index  Offset   Addend
        R_X86_64_64,       1,                0x20a4,  0x0000,
        END);
}

// Test the case of building a shared library with code that wasn't compiled with -fPIC.
// In this case, a R_X86_64_PC32 relocation cannot be processed
void test_dynamic_library_illegal_R_X86_64_PC32_relocation() {
    char *object_path1 = run_was(
        ".globl d;"
        ".data;"
        ".type d, @object;"
        ".size d, 4;"
        "d: .long 42;"
    );

    List *input_paths = new_list(1);
    append_to_list(input_paths, object_path1);

    char *lib_name;
    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_SHARED, &lib_name, 0, "dynamic_library_illegal_R_X86_64_PC32_relocation");
    char *lib_filename = malloc(strlen(lib_name) + 16);
    sprintf(lib_filename, "lib%s.so", &lib_name[1]);

    char *object_path2 = run_was(
        ".text;"
        ".globl _start;"
        "_start: movl d(%rip), %eax;"   // Creates a R_X86_64_PC32 relocation
    );

    input_paths = new_list(1);
    append_to_list(input_paths, object_path2);
    append_to_list(input_paths, lib_name);

    char output_template[] = "/tmp/asm_XXXXXX";
    int fd = mkstemp(output_template);
    if (fd == -1) { perror("mkstemp"); exit(1); }
    close(fd);

    List *extra_args = new_list(1);
    append_to_list(extra_args, "-shared");
    char *stderr_output = run_wld_and_capture_stderr(input_paths, extra_args, output_template);

    const char *expected = "error: A R_X86_64_PC32 relocation cannot be used for symbol \"d\" when making a shared object; recompile with -fPIC\n";
    assert_string(expected, stderr_output, "dynamic_library_illegal_R_X86_64_PC32_relocation");
    free(stderr_output);
}

void test_dynamic_executable_GOT_AND_PLT_relocation() {
    char *object_path = run_was(
        ".globl f;"
        "f: ret;"
    );

    List *input_paths = new_list(1);
    append_to_list(input_paths, object_path);

    char *lib_name;
    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_SHARED, &lib_name, 0, "dynamic_executable_GOT_AND_PLT_relocation");
    char *lib_filename = malloc(strlen(lib_name) + 16);
    sprintf(lib_filename, "lib%s.so", &lib_name[1]);

    // Make a second lib that uses the two ints from the first
    object_path = run_as(NULL,
        ".text;"
        "call *f@GOTPCREL(%rip);" // R_X86_64_GOTPCRELX
        "call f;" //# R_X86_64_PLT32
    );

    input_paths = new_list(1);
    append_to_list(input_paths, object_path);
    append_to_list(input_paths, lib_name);

    elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_SHARED, &lib_name, 0, "dynamic_executable_GOT_AND_PLT_relocation");

    assert_sections(elf_file,
        // Name            Type            Address   Offset  Size   Flags                      Align
        ".hash",           SHT_HASH,       0x1000,   0x1000, 0x14,  SHF_ALLOC,                 8,
        ".dynsym",         SHT_DYNSYM,     0x1014,   0x1014, 0x30,  SHF_ALLOC,                 1,
        ".dynstr",         SHT_STRTAB,     0x1044,   0x1044, 0x13,  SHF_ALLOC,                 1,
        ".rela.dyn",       SHT_RELA,       0x1058,   0x1058, 0x18,  SHF_ALLOC,                 8,
        ".rela.plt",       SHT_RELA,       0x2000,   0x2000, 0x18,  SHF_ALLOC | SHF_INFO_LINK, 8,
        ".plt",            SHT_PROGBITS,   0x3000,   0x3000, 0x20,  SHF_ALLOC | SHF_EXECINSTR, 8,
        ".text",           SHT_PROGBITS,   0x3020,   0x3020, 0x0b,  SHF_ALLOC | SHF_EXECINSTR, 1,
        ".dynamic",        SHT_DYNAMIC,    0x4000,   0x4000, 0xf0,  SHF_ALLOC | SHF_WRITE,     8,
        ".got",            SHT_PROGBITS,   0x40f0,   0x40f0, 0x08,  SHF_ALLOC | SHF_WRITE,     8,
        ".got.plt",        SHT_PROGBITS,   0x40f8,   0x40f8, 0x20,  SHF_ALLOC | SHF_WRITE,     8,
        NULL
    );

    // Relocation for call *f@GOTPCREL(%rip)
    assert_rela_dyn_relocations(elf_file,
        // Tag             Dyn symtab index  Offset   Addend
        R_X86_64_GLOB_DAT, 1,                0x40f0,  0x0000,
        END);

    // Relocation for call f
    assert_rela_plt_relocations(elf_file,
        // Tag              Dyn symtab index  Offset   Addend
        R_X86_64_JUMP_SLOT, 1,                0x4110,  0x0000,
        END);

    assert_dynsym(elf_file,
    //  Value      Size   Type        Binding     Visibility   Section    Name
        0x0000,    0,     STT_NOTYPE, STB_GLOBAL, STV_DEFAULT, "UND",     "f",
        END);
}

void test_versioning_default_symbol() {
    char *object_path1 = run_as(NULL,
        ".text;"
        ".globl f1_1;"
        ".type f1_1, @function;"
        "f1_1:"
        "    mov $1, %eax;"
        "    ret;"
        ".globl f1_2;"
        ".type f1_2, @function;"
        "f1_2:"
        "    mov $2, %eax;"
        "    ret;"
        ".symver f1_1,f1@@V1;"
        ".symver f1_2,f1@V2;"
    );

    char map_path[] = "/tmp/vermap_XXXXXX";
    int fd = mkstemp(map_path);
    if (fd == -1) { perror("mkstemp"); exit(1); }
    FILE *map_file = fdopen(fd, "w");
    if (!map_file) { perror("fdopen"); exit(1); }
    fprintf(map_file,
        "V1 {\n"
        "    global:\n"
        "        f1;\n"
        "};\n"
        "\n"
        "V2 {\n"
        "    global:\n"
        "        f1;\n"
        "};\n"
    );
    fclose(map_file);

    char lib_path[] = "/tmp/libtest.so.XXXXXX";
    int lib_fd = mkstemp(lib_path);
    if (lib_fd < 0) { perror("mkstemp failed"); exit(1); }
    close(lib_fd);

    char command[512];
    snprintf(command, sizeof(command), "ld -shared -o %s %s --soname libtest.so --version-script=%s", lib_path, object_path1, map_path);
    int result = system(command);
    if (result) {
        printf("ld failed with exit code %d\n", result >> 8);
        exit(1);
    }

    char *object_path2 = run_as(NULL,
        ".globl _start;"
        ".text;"
        "_start:;"
        "    call f1;"
    );

    List *input_paths = new_list(2);
    append_to_list(input_paths, object_path2);
    append_to_list(input_paths, "*test");

    OutputElfFile *elf_file = run_wld(input_paths, OUTPUT_TYPE_FLAG_SHARED | OUTPUT_TYPE_FLAG_EXECUTABLE, NULL, 0, "versioning_default_symbol");

    assert_sections(elf_file,
        // Name            Type             Address   Offset  Size   Flags                      Align
        ".hash",           SHT_HASH,        0x1000,   0x1000, 0x14,  SHF_ALLOC,                 8,
        ".dynsym",         SHT_DYNSYM,      0x1014,   0x1014, 0x30,  SHF_ALLOC,                 1,
        ".dynstr",         SHT_STRTAB,      0x1044,   0x1044, 0x1d,  SHF_ALLOC,                 1,
        ".gnu.version",    SHT_GNU_VERSYM,  0x1062,   0x1062, 0x04,  SHF_ALLOC,                 2,
        ".gnu.version_r",  SHT_GNU_VERNEED, 0x1068,   0x1068, 0x20,  SHF_ALLOC,                 8,
        ".rela.plt",       SHT_RELA,        0x2000,   0x2000, 0x30,  SHF_ALLOC | SHF_INFO_LINK, 8,
        ".plt",            SHT_PROGBITS,    0x3000,   0x3000, 0x30,  SHF_ALLOC | SHF_EXECINSTR, 8,
        ".text",           SHT_PROGBITS,    0x3030,   0x3030, 0x05,  SHF_ALLOC | SHF_EXECINSTR, 1,
        ".dynamic",        SHT_DYNAMIC,     0x4000,   0x4000, 0xf0,  SHF_ALLOC | SHF_WRITE,     8,
        ".got.plt",        SHT_PROGBITS,    0x40f0,   0x40f0, 0x28,  SHF_ALLOC | SHF_WRITE,     8,
        NULL
    );

    assert_dynamic(elf_file,
        DT_NEEDED,      0,      "libtest.so",
        DT_STRTAB,      0x1044, NULL,
        DT_SYMTAB,      0x1014, NULL,
        DT_STRSZ,       0x1d,   NULL,
        DT_SYMENT,      0x18,   NULL,
        DT_HASH,        0x1000, NULL,
        DT_VERNEED,     0x1068, NULL,
        DT_VERNEEDNUM,  0x1,    NULL,
        DT_VERSYM,      0x1062, NULL,
        DT_PLTGOT,      0x40f0, NULL,
        DT_PLTRELSZ,    0x30,   NULL,
        DT_PLTREL,      0x7,    NULL,
        DT_JMPREL,      0x2000, NULL,
        DT_DEBUG,       0,      NULL,
        DT_NULL,        0,      NULL
    );

    assert_dynsym(elf_file,
    //  Value      Size   Type        Binding     Visibility   Section    Name
        0x0000,    0,     STT_FUNC,   STB_GLOBAL, STV_DEFAULT, "UND",     "f1",
        END);

    // Check versym .gnu.version entries
    assert_section_data(elf_file, ".gnu.version",
        0x00, 0x00, // null
        0x02, 0x00, // f1 has V1, index 2
        END);

    // Check verneed .gnu.version_r
    OutputSection* verneed_section = get_output_section(elf_file, ".gnu.version_r");
    if (!verneed_section) panic("Did not find .gnu.version_r");
    OutputSection *dynstr_section = get_output_section(elf_file, DYNSTR_SECTION_NAME);
    if (!dynstr_section) panic("No .dynstr section");

    // Expect one filename "libtest.so", with one version "V1"
    ElfVerneed *vn = (ElfVerneed *) verneed_section->data;
    ElfVernaux *vna = (ElfVernaux *) (verneed_section->data + sizeof(ElfVerneed));

    assert_string("libtest.so", &dynstr_section->data[vn->vn_file], "Filename is libtest.so");
    assert_int(1, vn->vn_cnt, "1 .gnu_version_r entry");
    assert_int(0, vn->vn_next, ".gnu_version_r has one filename");
    assert_string("V1", &dynstr_section->data[vna->vna_name], "Version is V1");
    assert_int(2, vna->vna_other, "libtest.so V1 has index 2");
    assert_int(0, vna->vna_next, "libtest.so has one version");
}

int main() {
    test_sanity();
    test_empty_object_file();
    test_segments_are_page_aligned();
    test_orphan_sections_rearrangement();
    test_orphan_sections_no_rearrangement();
    test_tls();
    test_tls_aligment();
    test_two_bss_sections();
    test_data_and_two_bss_sections();
    test_etext_undefined();
    test_defined_etext();
    test_unused_etext();
    test_undefined_symbol();
    test_automatic_start_stop_symbols();
    test_shared_library_no_dependencies();
    test_two_shared_libs_with_data();
    test_two_shared_libs_with_functions();
    test_shared_lib_with_two_objects();
    test_dynamic_executable_with_GOT_entry_for_local_symbol();
    test_dynamic_executable_with_GOT_entry_for_local_symbol_and_GOT_relocation_too();
    test_dwarf();
    test_gnu_ld_script_archive();
    test_dynamic_executable_sanity();
    test_dynamic_executable_using_an_object_in_a_library();
    test_dynamic_executable_R_X86_64_RELATIVE_relocations_from_rodata();
    test_dynamic_executable_R_X86_64_RELATIVE_relocations_from_initialized_data();
    test_dynamic_executable_R_X86_64_RELATIVE_relocations_from_uninitialized_data();
    test_dynamic_executable_R_X86_64_RELATIVE_relocations_from_two_files();
    test_dynamic_executable_R_X86_64_64_relocation();
    test_dynamic_library_R_X86_64_64_relocation_for_function_pointer();
    test_dynamic_library_illegal_R_X86_64_PC32_relocation();
    test_dynamic_library_R_X86_64_64_relocation_when_making_a_shared_library();
    test_dynamic_executable_GOT_AND_PLT_relocation();
    test_versioning_default_symbol();
}
