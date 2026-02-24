#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "list.h"
#include "input-elf.h"
#include "output-elf.h"
#include "strmap-ordered.h"

#include "wld/layout.h"
#include "wld/lexer.h"
#include "wld/parser.h"
#include "wld/libs.h"
#include "wld/relocations.h"
#include "wld/script.h"
#include "wld/symbols.h"
#include "wld/utils.h"
#include "wld/wld.h"

static char *entrypoint_symbol_name;

static const char *SECTION_TYPE_NAMES[] = {
    "NULL",
    "PROGBITS",
    "SYMTAB",
    "STRTAB",
    "RELA",
    "HASH",
    "DYNAMIC",
    "NOTE",
    "NOBITS",
    "REL",
    "SHLIB",
    "DYNSYM",
    "UNKNOWN",
    "UNKNOWN",
    "INIT_ARRAY",
    "FINI_ARRAY",
    "PREINIT_ARRAY",
    "GROUP",
    "SYMTAB_SHNDX",
    "NUM",
};

static const char *PROGRAM_SEGMENT_TYPE_NAMES[] = { "NULL", "LOAD", "DYNAMIC", "INTERP", "NOTE", "SHLIB", "PHDR", "TLS", "NUM" };

const char *DYNAMIC_SECTION_TYPE_NAMES[] = {
    "NULL",
    "NEEDED",
    "PLTRELSZ",
    "PLTGOT",
    "HASH",
    "STRTAB",
    "SYMTAB",
    "RELA",
    "RELASZ",
    "RELAENT",
    "STRSZ",
    "SYMENT",
    "INIT",
    "FINI",
    "SONAME",
    "RPATH",
    "SYMBOLIC",
    "REL",
    "RELSZ",
    "RELENT",
    "PLTREL",
    "DEBUG",
    "TEXTREL",
    "JMPREL",
    "BIND_NOW",
    "INIT_ARRAY",
    "FINI_ARRAY",
    "INIT_ARRAYSZ",
    "FINI_ARRAYSZ",
    "RUNPATH",
    "FLAGS",
    "ENCODING",
    "PREINIT_ARRAY",
    "PREINIT_ARRAYSZ",
    "SYMTAB_SHNDX",
};

const char *dynamic_section_name(uint64_t tag) {
    uint64_t count = sizeof(DYNAMIC_SECTION_TYPE_NAMES) / sizeof(DYNAMIC_SECTION_TYPE_NAMES[0]);
    if (tag < count) return DYNAMIC_SECTION_TYPE_NAMES[tag];
    if (tag == DT_VERSYM) return "VERSYM";
    if (tag == DT_VERNEED) return "VERNEED";
    if (tag == DT_VERNEEDNUM) return "VERNEEDNUM";
    return "UNKNOWN";
}

const char *section_type_name(uint64_t type) {
    uint64_t count = sizeof(SECTION_TYPE_NAMES) / sizeof(SECTION_TYPE_NAMES[0]);
    if (type < count) return SECTION_TYPE_NAMES[type];
    if (type == SHT_GNU_VERSYM) return "GNU_VERSYM";
    if (type == SHT_GNU_VERNEED) return "GNU_VERNEED";
    if (type == SHT_GNU_VERDEF) return "GNU_VERDEF";
    return "UNKNOWN";
}

static void run_linker_script(const char *path, List *input_elf_files, StrMap *read_shared_object_files, List *library_paths);

OutputElfFile *init_output_elf_file(const char *output_filename, int output_type) {
    int elf_output_type = output_type & OUTPUT_TYPE_FLAG_STATIC ? ET_EXEC : ET_DYN;

    OutputElfFile *result = new_output_elf_file(output_filename, elf_output_type);
    result->extra_sections = new_strmap_ordered();
    result->ifunc_symbols = new_list(0);
    result->global_symbols_in_use  = new_strmap();
    result->is_executable = output_type & OUTPUT_TYPE_FLAG_EXECUTABLE;
    result->rela_dyn_R_X86_64_RELATIVE_relocations = new_list(32);
    result->rela_dyn_R_X86_64_64_relocations = new_list(32);

    return result;
}

// Get an extra section. Returns null if it doesn't exist
InputSection *get_extra_section(OutputElfFile *output_elf_file, char *name) {
    InputSection *section = strmap_ordered_get(output_elf_file->extra_sections, name);
    return section;
}

// Create an extra section. It must not already exist.
InputSection *create_extra_section(OutputElfFile *output_elf_file, char *name, uint32_t type, uint64_t flags, uint64_t align) {
    if (strmap_ordered_get(output_elf_file->extra_sections, name))
        panic("Extra section %s already exists", name);

    InputSection *extra_section = calloc(1, sizeof(InputSection));
    extra_section->name = strdup(name);
    extra_section->type = type;
    extra_section->flags = flags;
    extra_section->align = align;
    strmap_ordered_put(output_elf_file->extra_sections, strdup(name), extra_section);

    return extra_section;
}

InputSection *get_or_create_extra_section(OutputElfFile *output_elf_file, char *name, uint32_t type, uint64_t flags, uint64_t align) {
    InputSection *section = get_extra_section(output_elf_file, name);
    if (section) return section;
    return create_extra_section(output_elf_file, name, type, flags, align);
}

// Go down all input files which are either object files or shared libraries
static void read_object_or_shared_library_file(List *input_elf_files, const char *path) {
    if (DEBUG_SYMBOL_RESOLUTION || DEBUG_SYMBOL_VERSIONS)
        printf("Examining file %s\n", path);

    // Determine if the file is a shared object or object.
    // This allows the user to pass a shared library nane, e.g. testfoo.so to be passed on the command
    // line instead of -lfoo.
    int source;
    FileType type = identify_library_file(path);
    switch (type) {
        case FT_SHARED_LIBRARY:
            source = SRC_SHARED_LIBRARY;
            break;
        default:
            source = SRC_OBJECT;
    }

    InputElfFile *elf_file = open_elf_file(path);
    process_elf_file_symbols(elf_file, source, 0);
    append_to_list(input_elf_files, elf_file);
}

// Identify the filetype and load the file
static int read_input_file(const char *path, List *input_elf_files, StrMap *read_shared_object_files, List *library_paths) {
    // If the path isn't absolute, search for it
    if (path[0] != '/') {
        path = find_file(library_paths, path, "library");
        if (!path)
            error("Unable to find file: %s", path);
    }

    int objects_added = 0;

    FileType type = identify_library_file(path);

    // Only read an .so file once
    if (type == FT_SHARED_LIBRARY) {
        if (strmap_get(read_shared_object_files, path)) {
            return 0;
        }

        strmap_put(read_shared_object_files, path, (void *) 1);
    }

    switch (type) {
        case FT_LINKER_SCRIPT:
            run_linker_script(path, input_elf_files, read_shared_object_files, library_paths);
            break;

        case FT_ARCHIVE: {
            ArchiveFile *ar_file = open_archive_file(path);
            objects_added = process_library_symbols(ar_file, input_elf_files);
            break;
        }

        case FT_SHARED_LIBRARY:
            read_object_or_shared_library_file(input_elf_files, path);
            break;

        default:
            panic("Unhandled library type");
    }

    return objects_added;
}

// Static libraries such as libm.a can consist of a linker script that looks something like:
// /* GNU ld script
// */
// OUTPUT_FORMAT(elf64-x86-64)
// GROUP ( /usr/lib/x86_64-linux-gnu/libm-2.31.a /usr/lib/x86_64-linux-gnu/libmvec.a )
// Parse the linker script and process all libraries in the GROUP()
static void run_linker_script(const char *path, List *input_elf_files, StrMap *read_shared_object_files, List *library_paths) {
    init_lexer(path);
    List *linker_script = parse();

    // Loop over the group
    for (int i = 0; i < linker_script->length; i++) {
        ScriptCommand *command = linker_script->elements[i];
        if (command->type == CMD_INPUT) {
            // Loop over the input until no more objects are added
            List *items = command->input.items;
            for (int j = 0; j < items->length; j++) {
                InputGroupItem *input_group_item = items->elements[j];
                char *path = input_group_item->filename;

                // Handle -lfoo
                int length = strlen(path);
                if (length > 2 && path[0] == '-' && path[1] == 'l') {
                    // Transform -lfoo to libfoo.a
                    char *new_path = malloc(length + 4);
                    sprintf(new_path, "lib%s.a", &path[2]);
                    path = new_path;
                }

                read_input_file(path, input_elf_files, read_shared_object_files, library_paths);
            }
        }

        else if (command->type == CMD_GROUP) {
            // Loop over the group until no more objects are added
            while (1) {
                int objects_added = 0;

                List *items = command->group.items;
                for (int j = 0; j < items->length; j++) {
                    InputGroupItem *input_group_item = items->elements[j];
                    char *path = input_group_item->filename;
                    objects_added += read_input_file(path, input_elf_files, read_shared_object_files, library_paths);
                }

                if (!objects_added) break;
            }
        }
    }
}

// Go down all input files which are either object files or libraries
static List *read_input_files(List *library_paths, List *input_files, int output_type) {
    List *input_elf_files = new_list(32);
    StrMap *read_shared_object_files = new_strmap();

    for (int i = 0; i < input_files->length; i++) {
        InputFile *input_file = input_files->elements[i];
        char *input_filename = input_file->filename;
        if (DEBUG_SYMBOL_RESOLUTION) printf("Examining file %s\n", input_filename);

        if (input_file->is_library_name) {
            const char *path = search_for_library(output_type, library_paths, input_filename);
            read_input_file(path, input_elf_files, read_shared_object_files, library_paths);
        }
        else {
            // It's an object file
            read_object_or_shared_library_file(input_elf_files, input_filename);
        }
    }

    free_strmap(read_shared_object_files);

    if (DEBUG_SYMBOL_VERSIONS) debug_print_global_symbol_version_indexes();

    return input_elf_files;
}

// Go through the linker script and set the entrypoint symbol
static void set_entrypoint_symbol(OutputElfFile *output_elf_file, List *input_elf_files) {
    if (!output_elf_file->is_executable) return;

    for (int i = 0; i < output_elf_file->linker_script->length; i++) {
        ScriptCommand *script_command = output_elf_file->linker_script->elements[i];

        if (script_command->type == CMD_ENTRY)
            entrypoint_symbol_name = script_command->entry.symbol;
    }
}

static void make_shared_libraries_list(OutputElfFile *output_elf_file, List *input_elf_files) {
    output_elf_file->shared_libraries = new_list(8);

    for (int i = 0; i < input_elf_files->length; i++) {
        InputElfFile *elf_file = input_elf_files->elements[i];
        if (elf_file->type == ET_DYN) {
            char *filename = strdup(elf_file->filename);

            // Strip off any leading paths
            char *p = strrchr(filename, '/');
            if (p) filename = p + 1;

            append_to_list(output_elf_file->shared_libraries, filename);
        }
    }
}

// Print .dynamic section in a format similar to readelf's output
void dump_dynamic_section(OutputElfFile *output_elf_file) {
    InputSection *section_dynamic = get_extra_section(output_elf_file, DYNAMIC_SECTION_NAME);
    if (!section_dynamic) panic("Expected a dynamic section in dump_dynamic_section");
    if (!section_dynamic->output_section) panic("Expected a dynamic section with output");

    InputSection *section_dynstr = get_extra_section(output_elf_file, DYNSTR_SECTION_NAME);
    if (!section_dynstr) panic("Expected a dynstr in dump_dynamic_section");
    if (!section_dynstr->output_section) panic("Expected a dynstr section with output");

    uint64_t offset = section_dynamic->output_section->offset + section_dynamic->src_offset;
    uint64_t length = section_dynamic->size / sizeof(ElfDyn);
    printf("Dynamic section at offset %#lx contains %ld entries:\n", offset, length);
    printf("[Nr] Type                Name/Value\n");

    for (int i = 0; i < length; i++) {
        ElfDyn *dyn = &((ElfDyn *) section_dynamic->data)[i];
        uint64_t tag = dyn->d_tag & 0xffffffff;
        uint64_t val = dyn->d_un.d_val;

        const char *tag_name = dynamic_section_name(tag);

        if (tag == DT_NEEDED) {
            char *symbol_name = &((char *) section_dynstr->data)[val];
            printf("%3d %-20s Shared library: %s\n", i, tag_name, symbol_name);
        } else {
            printf("%3d %-20s %#-10lx\n", i, tag_name, val);
        }
    }
}

void dump_relocations(OutputSection* section) {
    printf("Relocations:\n");
    printf("info          offset   addend\n");
    ElfRelocation *relocations = (ElfRelocation *) section->data;

    int count = section->size / sizeof(ElfRelocation);
    for (int i = 0; i < count; i++) {
        ElfRelocation *r = &relocations[i];
        printf("%#8lx %#8lx %8ld\n", r->r_info, r->r_offset, r->r_addend);
    }
}

static OutputSection *get_non_empty_output_section(OutputElfFile *output_elf_file, char *name) {
    OutputSection *section = get_output_section(output_elf_file, name);
    if (!section || section->size == 0) return NULL;
    return section;
}

static void make_dynamic_section_entry_count(OutputElfFile *output_elf_file, char *soname) {
    int dynamic_section_entry_count = BASE_DYNAMIC_SECTION_ENTRY_COUNT + output_elf_file->shared_libraries->length;

    if (get_non_empty_output_section(output_elf_file, INIT_ARRAY_SECTION_NAME)) dynamic_section_entry_count += 2;
    if (get_non_empty_output_section(output_elf_file, FINI_ARRAY_SECTION_NAME)) dynamic_section_entry_count += 2;
    if (output_elf_file->verneed_names && output_elf_file->verneed_names->length > 0) dynamic_section_entry_count += 2;
    if (get_extra_section(output_elf_file, VERSYM_SECTION_NAME)) dynamic_section_entry_count += 1;
    if (output_elf_file->rela_dyn_entry_count > 0) dynamic_section_entry_count += 3;
    if (output_elf_file->got_plt_entries_count > 0) dynamic_section_entry_count += 4;
    if (soname) dynamic_section_entry_count++;

    output_elf_file->dynamic_section_entry_count = dynamic_section_entry_count;
}

static void set_in_dynamic_section(OutputElfFile *output_elf_file, int index, int64_t tag, uint64_t val_or_addr) {
    InputSection *section_dynamic = get_extra_section(output_elf_file, DYNAMIC_SECTION_NAME);

    if (index >= output_elf_file->dynamic_section_entry_count)
        panic("Exceeded dynamic_section_entry_count=%d", output_elf_file->dynamic_section_entry_count);

    ElfDyn entry = {.d_tag = tag, .d_un.d_val = val_or_addr};
    ElfDyn *entries = section_dynamic->data; // = realloc(section_dynamic->data, section_dynamic->size + sizeof(ElfDyn));
    entries[index] = entry;
}

static void create_interp_section(OutputElfFile *output_elf_file, char *dynamic_linker) {
    if (!dynamic_linker) return;

    InputSection *section_dynamic = create_extra_section(output_elf_file, INTERP_SECTION_NAME, SHT_PROGBITS, SHF_ALLOC, 1);
    section_dynamic->size = strlen(dynamic_linker) + 1;
    section_dynamic->data = dynamic_linker;
}

// Add several sections if it's a shared library
static void create_dynamic_sections(OutputElfFile *output_elf_file, List *input_elf_files, char *soname) {
    if (output_elf_file->type != ET_DYN) return;

    make_shared_libraries_list(output_elf_file, input_elf_files);
    make_dynamic_section_entry_count(output_elf_file, soname);

    InputSection *section_dynamic = create_extra_section(output_elf_file, DYNAMIC_SECTION_NAME, SHT_DYNAMIC, SHF_ALLOC | SHF_WRITE, 8);
    section_dynamic->size = output_elf_file->dynamic_section_entry_count * sizeof(ElfDyn);
    section_dynamic->data = calloc(1, section_dynamic->size);

    output_elf_file->section_hash = create_extra_section(output_elf_file, HASH_SECTION_NAME, SHT_HASH, SHF_ALLOC, 8);

    int pos = 0;
    if (soname) {
        int dynstr_offset = add_dynstr_string(output_elf_file, soname);
        set_in_dynamic_section(output_elf_file, pos++, DT_SONAME, dynstr_offset);
    }

    int shlib_len = output_elf_file->shared_libraries->length;
    for (int i = 0; i < shlib_len; i++) {
        char *filename = output_elf_file->shared_libraries->elements[i];
        int dynstr_offset = add_dynstr_string(output_elf_file, filename);
        set_in_dynamic_section(output_elf_file, pos++, DT_NEEDED, dynstr_offset);
    }
}

// Update values in .dynamic section
static void update_dynamic_sections(OutputElfFile *output_elf_file, char *soname) {
    if (output_elf_file->type != ET_DYN) return;

    InputSection *section_dynamic = get_extra_section(output_elf_file, DYNAMIC_SECTION_NAME);
    InputSection *section_dynstr = output_elf_file->section_dynstr;
    InputSection *section_dynsym = output_elf_file->section_dynsym;
    InputSection *section_hash = output_elf_file->section_hash;
    InputSection *section_rela_dyn = output_elf_file->section_rela_dyn;
    InputSection *section_rela_plt = output_elf_file->section_rela_plt;
    InputSection *section_got_plt = output_elf_file->section_got_plt;
    InputSection *section_verneed = get_extra_section(output_elf_file, VERNEED_SECTION_NAME);
    InputSection *section_versym = get_extra_section(output_elf_file, VERSYM_SECTION_NAME);
    OutputSection *section_init_array = get_non_empty_output_section(output_elf_file, INIT_ARRAY_SECTION_NAME);
    OutputSection *section_fini_array = get_non_empty_output_section(output_elf_file, FINI_ARRAY_SECTION_NAME);

    int pos = output_elf_file->shared_libraries->length;
    if (soname) pos++;

    set_in_dynamic_section(output_elf_file, pos++, DT_STRTAB, section_dynstr->output_section->address);
    set_in_dynamic_section(output_elf_file, pos++, DT_SYMTAB, section_dynsym->output_section->address);
    set_in_dynamic_section(output_elf_file, pos++, DT_STRSZ,  section_dynstr->size);
    set_in_dynamic_section(output_elf_file, pos++, DT_SYMENT, sizeof(ElfSymbol));
    set_in_dynamic_section(output_elf_file, pos++, DT_HASH, section_hash->output_section->address);

    if (section_init_array) {
        set_in_dynamic_section(output_elf_file, pos++, DT_INIT_ARRAY, section_init_array->address);
        set_in_dynamic_section(output_elf_file, pos++, DT_INIT_ARRAYSZ, section_init_array->size);
    }

    if (section_fini_array) {
        set_in_dynamic_section(output_elf_file, pos++, DT_FINI_ARRAY, section_fini_array->address);
        set_in_dynamic_section(output_elf_file, pos++, DT_FINI_ARRAYSZ, section_fini_array->size);
    }

    if (section_verneed && section_verneed->size > 0 && section_verneed->output_section) {
        set_in_dynamic_section(output_elf_file, pos++, DT_VERNEED, section_verneed->output_section->address);
        set_in_dynamic_section(output_elf_file, pos++, DT_VERNEEDNUM, section_verneed->info);
    }

    if (section_versym && section_versym->size > 0 && section_versym->output_section) {
        set_in_dynamic_section(output_elf_file, pos++, DT_VERSYM, section_versym->output_section->address);
    }

    if (output_elf_file->got_plt_entries_count > 0) {
        set_in_dynamic_section(output_elf_file, pos++, DT_PLTGOT,   section_got_plt->output_section->address);
        set_in_dynamic_section(output_elf_file, pos++, DT_PLTRELSZ, section_rela_plt->output_section->size);
        set_in_dynamic_section(output_elf_file, pos++, DT_PLTREL,   DT_RELA);
        set_in_dynamic_section(output_elf_file, pos++, DT_JMPREL,   section_rela_plt->output_section->address);
    }

    if (output_elf_file->rela_dyn_entry_count > 0) {
        if (!section_rela_dyn) panic("section_rela_dyn NULL");
        if (!section_rela_dyn->output_section) panic("section_rela_dyn->output_section is NULL");

        set_in_dynamic_section(output_elf_file, pos++, DT_RELA,    section_rela_dyn->output_section->address);
        set_in_dynamic_section(output_elf_file, pos++, DT_RELASZ,  section_rela_dyn->output_section->size);
        set_in_dynamic_section(output_elf_file, pos++, DT_RELAENT, sizeof(ElfRelocation));
    }

    set_in_dynamic_section(output_elf_file, pos++, DT_DEBUG, 0);
    set_in_dynamic_section(output_elf_file, pos++, DT_NULL, 0);
}

// Associate sections using sh_info
static void associate_sections(OutputElfFile *output_elf_file) {
    if (output_elf_file->type == ET_DYN) {
        InputSection *section_dynstr = output_elf_file->section_dynstr;

        InputSection *section_dynamic = get_extra_section(output_elf_file, DYNAMIC_SECTION_NAME);
        ElfSectionHeader *h = &output_elf_file->elf_section_headers[section_dynamic->output_section->index];
        h->sh_link = section_dynstr->output_section->index;

        InputSection *section_dynsym = output_elf_file->section_dynsym;
        h = &output_elf_file->elf_section_headers[section_dynsym->output_section->index];
        section_dynsym->output_section->link = section_dynstr->output_section->index;
        h->sh_link = section_dynsym->output_section->link;
        h->sh_info = 1; // The first non-local index. The 0th entry is the local null symbol
        h->sh_entsize = sizeof(ElfSymbol);

        InputSection *section_hash = output_elf_file->section_hash;
        h = &output_elf_file->elf_section_headers[section_hash->output_section->index];
        h->sh_link = section_dynsym->output_section->index;

        InputSection *section_verneed = get_extra_section(output_elf_file, VERNEED_SECTION_NAME);
        if (section_verneed) {
            section_verneed->output_section->link = section_dynstr->output_section->index;
            section_verneed->output_section->info = section_verneed->info;
            h = &output_elf_file->elf_section_headers[section_verneed->output_section->index];
            h->sh_link = section_verneed->output_section->link;
            h->sh_info = section_verneed->output_section->info;
            h->sh_entsize = sizeof(ElfVerneed);
        }

        InputSection *section_versym = get_extra_section(output_elf_file, VERSYM_SECTION_NAME);
        if (section_versym) {
            section_versym->output_section->link = section_dynsym->output_section->index;
            h = &output_elf_file->elf_section_headers[section_versym->output_section->index];
            h->sh_link = section_versym->output_section->link;
            h->sh_entsize = sizeof(uint16_t);
        }
    }
}

static void allocate_elf_output_memory(OutputElfFile *output_elf_file) {
    output_elf_file->data = calloc(1, output_elf_file->size);
}

// For dynamic executables, the first program segment header must be the program header.
static void prepend_phdr_program_segment_header(OutputElfFile *output_elf_file){
    ElfProgramSegmentHeader *phdr_segment = calloc(1, sizeof(ElfProgramSegmentHeader));

    phdr_segment->p_type   = PT_PHDR;
    phdr_segment->p_flags  = PF_R;
    phdr_segment->p_align  = 8;

    prepend_to_list(output_elf_file->program_segments_list, phdr_segment);
}

// Populate the first program segment header. This contains a page that has the start of the executable.
static void prepend_first_load_program_segment_header(OutputElfFile *output) {
    // The lowest address is in the first program segment header.
    // Use this address to find an address low enough to hold the ELF headers and round it down to a page.

    // Find first PT_LOAD segment
    // Executables have a PT_PHDR as the first entry, which must be skipped over.
    int first_load_segment_index = -1;
    for (int i = 0; i < output->program_segments_list->length; i++) {
        ElfProgramSegmentHeader *segment = output->program_segments_list->elements[i];
        if (segment->p_type == PT_LOAD) {
            first_load_segment_index = i;
            break;
        }
    }

    if (first_load_segment_index == -1) panic("No PT_LOAD sections in ELF file");

    ElfProgramSegmentHeader *first_program_segment = output->program_segments_list->elements[first_load_segment_index];
    uint64_t address = first_program_segment->p_paddr - headers_size(output);
    address = ALIGN_DOWN(address, 0x1000);

    uint64_t size = output->elf_section_headers_offset + output->elf_section_headers_size;

    ElfProgramSegmentHeader *h = calloc(1, sizeof(ElfProgramSegmentHeader));

    h->p_type   = PT_LOAD;  // Loadable
    h->p_flags  = PF_R;     // Read only
    h->p_vaddr  = address;
    h->p_paddr  = address;
    h->p_filesz = size;
    h->p_memsz  = size;
    h->p_align  = 0x1000;   // Align on page boundaries

    prepend_to_list(output->program_segments_list, h);
}

// Print sections in a format similar to readelf's output
void dump_sections(OutputElfFile *output_elf_file) {
    printf("Sections:\n");
    printf("[Nr] Name                          Type            Address          Off      Size     ES Flg Lk Inf Al\n");

    for (int i = 0; i < output_elf_file->sections_list->length; i++) {
        OutputSection *section = output_elf_file->sections_list->elements[i];

        char flags[10] = {0};
        char *p = flags;
        if (section->flags & SHF_WRITE)     *p++ = 'W';
        if (section->flags & SHF_ALLOC)     *p++ = 'A';
        if (section->flags & SHF_EXECINSTR) *p++ = 'X';
        if (section->flags & SHF_MERGE)     *p++ = 'M';
        if (section->flags & SHF_STRINGS)   *p++ = 'S';
        if (section->flags & SHF_INFO_LINK) *p++ = 'I';
        if (section->flags & SHF_GROUP)     *p++ = 'G';
        if (section->flags & SHF_TLS)       *p++ = 'T';
        *p = '\0';

        printf("%4d %-29s %-14s  %016lx %08lx %08lx %02lx %-3s %2d %2d %3d\n",
            i, section->name, section_type_name(section->type), section->address, section->offset,
            section->size, section->entsize, flags, section->link, section->info, section->align);
    }
}

// Print sections in a format similar to readelf's output
void dump_program_segments(OutputElfFile *output_elf_file) {
    printf("Program Segments:\n");
    printf("[Nr] Type           Offset   VirtAddr         PhysAddr         FileSiz  MemSiz   Flg Align\n");

    for (int i = 0; i < output_elf_file->program_segments_list->length; i++) {
        ElfProgramSegmentHeader *program_segment = output_elf_file->program_segments_list->elements[i];

        printf("%4d %-14s %08lx %016lx %016lx %08lx %08lx %c%c%c %06lx\n",
            i, PROGRAM_SEGMENT_TYPE_NAMES[program_segment->p_type],
            program_segment->p_offset, program_segment->p_vaddr, program_segment->p_vaddr,
            program_segment->p_filesz, program_segment->p_memsz,
            (program_segment->p_flags & PF_R) ? 'R' : ' ',
            (program_segment->p_flags & PF_W) ? 'W' : ' ',
            (program_segment->p_flags & PF_X) ? 'E' : ' ',
            program_segment->p_align
        );
    }
}

// Make the ELF program segment headers. Include the special TLS section if required.
static void make_elf_program_segment_headers(OutputElfFile *output_elf_file, char *dynamic_linker) {
    if (dynamic_linker) {
        InputSection *interp_section = get_extra_section(output_elf_file, INTERP_SECTION_NAME);

        ElfProgramSegmentHeader *dl_program_segment = calloc(1, sizeof(ElfProgramSegmentHeader));

        dl_program_segment->p_type   = PT_INTERP;
        dl_program_segment->p_flags  = PF_R;
        dl_program_segment->p_align  = 1;
        dl_program_segment->p_filesz = interp_section->size;
        dl_program_segment->p_memsz  = interp_section->size;
        dl_program_segment->p_offset = interp_section->output_section->offset;
        dl_program_segment->p_vaddr  = interp_section->output_section->address;
        dl_program_segment->p_paddr  = interp_section->output_section->address;

        append_to_list(output_elf_file->program_segments_list, dl_program_segment);
    }

    int needs_tls = 0;

    for (int i = 0; i < output_elf_file->sections_list->length; i++) {
        OutputSection *section = output_elf_file->sections_list->elements[i];
        if (section->flags & SHF_TLS) needs_tls = 1;
    }

    if (needs_tls) {
        ElfProgramSegmentHeader *tls_program_segment = calloc(1, sizeof(ElfProgramSegmentHeader));

        tls_program_segment->p_type = PT_TLS;
        tls_program_segment->p_flags = PF_R;
        tls_program_segment->p_offset = output_elf_file->tls_template_offset;
        tls_program_segment->p_filesz = output_elf_file->tls_template_tdata_size;
        tls_program_segment->p_memsz = output_elf_file->tls_template_tdata_size + output_elf_file->tls_template_tbss_size;
        tls_program_segment->p_vaddr = output_elf_file->tls_template_address;
        tls_program_segment->p_paddr = output_elf_file->tls_template_address;
        tls_program_segment->p_align = 8;

        append_to_list(output_elf_file->program_segments_list, tls_program_segment);
    }

    if (output_elf_file->type == ET_DYN) {
        InputSection *section_dynamic = get_extra_section(output_elf_file, DYNAMIC_SECTION_NAME);

        ElfProgramSegmentHeader *tls_program_segment = calloc(1, sizeof(ElfProgramSegmentHeader));

        tls_program_segment->p_type = PT_DYNAMIC;
        tls_program_segment->p_flags = PF_R | PF_W;
        tls_program_segment->p_offset = section_dynamic->output_section->offset;
        tls_program_segment->p_filesz = section_dynamic->output_section->size;
        tls_program_segment->p_memsz = section_dynamic->output_section->size;
        tls_program_segment->p_vaddr = section_dynamic->output_section->address;
        tls_program_segment->p_paddr = section_dynamic->output_section->address;
        tls_program_segment->p_align = 8;

        append_to_list(output_elf_file->program_segments_list, tls_program_segment);
    }

    // One to two extra entries are prepended here. A PT_LOAD segment for the start of the executable,
    // and, for dynamic executables, a PT_PHDR segment
    int is_dyn_executable = output_elf_file->is_executable && output_elf_file->type == ET_DYN;

    // Populate the first program segment header that loads the start of the executable
    prepend_first_load_program_segment_header(output_elf_file);

    if (is_dyn_executable) prepend_phdr_program_segment_header(output_elf_file);

    // Allocate memory for the program segment headers
    output_elf_file->elf_program_segments_count = output_elf_file->program_segments_list->length;
    output_elf_file->elf_program_segments_header_size = sizeof(ElfProgramSegmentHeader) * output_elf_file->elf_program_segments_count;
    output_elf_file->elf_program_segment_headers = calloc(1, output_elf_file->elf_program_segments_header_size);
    output_elf_file->elf_program_segments_offset  = sizeof(ElfSectionHeader);
    output_elf_file->elf_section_headers_offset  = output_elf_file->elf_program_segments_offset + output_elf_file->elf_program_segments_header_size;

    // Fixup the now-known values in PHDR
    if (is_dyn_executable) {
        ElfProgramSegmentHeader *phdr_segment = output_elf_file->program_segments_list->elements[0];
        phdr_segment->p_filesz = output_elf_file->elf_program_segments_header_size;
        phdr_segment->p_memsz  = phdr_segment->p_filesz;
        phdr_segment->p_offset = output_elf_file->elf_program_segments_offset;
        phdr_segment->p_vaddr  = phdr_segment->p_offset;
        phdr_segment->p_paddr  = phdr_segment->p_offset;
    }

    for (int i = 0; i < output_elf_file->program_segments_list->length; i++) {
        ElfProgramSegmentHeader *program_segment = output_elf_file->program_segments_list->elements[i];
        memcpy(&output_elf_file->elf_program_segment_headers[i], program_segment, sizeof(ElfProgramSegmentHeader));
    }

    if (DEBUG_LAYOUT) dump_program_segments(output_elf_file);

    // Copy program headers
    memcpy(
        output_elf_file->data + output_elf_file->elf_program_segments_offset,
        output_elf_file->elf_program_segment_headers,
        output_elf_file->elf_program_segments_header_size);
}

// Add some sections that are always present in output ELF file
static void create_default_sections(OutputElfFile *output_elf_file) {
    add_output_section(output_elf_file, "" , SHT_NULL, 0, 0);

    output_elf_file->section_symtab      = add_output_section(output_elf_file, ".symtab",   SHT_SYMTAB, 0, 8);
    output_elf_file->section_strtab      = add_output_section(output_elf_file, ".strtab",   SHT_STRTAB, 0, 1);
    output_elf_file->section_shstrtab    = add_output_section(output_elf_file, ".shstrtab", SHT_STRTAB, 0, 1);

    output_elf_file->section_symtab->entsize = sizeof(ElfSymbol);
    add_to_output_section(output_elf_file->section_strtab, "", 1);
}

// Process IFUNCs and create .got.plt, .iplt and .rela.iplt if required
static void process_ifuncs(OutputElfFile *output_elf_file, List *input_elf_files) {
    if (output_elf_file->type == ET_DYN) return;

    // Global symbols
    process_ifuncs_from_symbol_table(output_elf_file, global_symbol_table);

    // Local symbols
    for (int i = 0; i < input_elf_files->length; i++) {
        InputElfFile *elf_file = input_elf_files->elements[i];
        if (DEBUG_RELOCATIONS) printf("\nLocal symbols for %s:\n", elf_file->filename);
        SymbolTable *local_symbol_table = get_local_symbol_table(elf_file);
        process_ifuncs_from_symbol_table(output_elf_file, local_symbol_table);
    }
}

// Create the .bss section if it doesn't already exist
OutputSection *get_or_create_create_bss_section(OutputElfFile *output) {
    if (!output->section_bss)
        output->section_bss = add_output_section(output, ".bss", SHT_NOBITS, SHF_ALLOC | SHF_WRITE, 0);

    return output->section_bss;
}

// If there are any common symbols, create a bss section and allocate values for the symbols
static void add_common_symbols_to_bss(OutputElfFile *output) {
    if (!common_symbols_are_present()) return;

    get_or_create_create_bss_section(output);
    layout_common_symbols_in_bss_section(output->section_bss);
}

// Assign final values to all symbols
static void make_symbol_values(OutputElfFile *output_elf_file, List *input_elf_files) {
    if (DEBUG_RELOCATIONS) printf("\nGlobal symbols:\n");

    // Global symbols
    make_symbol_values_from_symbol_table(output_elf_file, global_symbol_table);

    // Local symbols
    for (int i = 0; i < input_elf_files->length; i++) {
        InputElfFile *elf_file = input_elf_files->elements[i];
        SymbolTable *local_symbol_table = get_local_symbol_table(elf_file);
        if (DEBUG_RELOCATIONS) printf("\nMaking symbol values for %s\n", elf_file->filename);
        make_symbol_values_from_symbol_table(output_elf_file, local_symbol_table);
    }
}

// Set the executable entrypoint
static void set_entrypoint(OutputElfFile *output_elf_file) {
    if (!output_elf_file->is_executable) return;

    if (!entrypoint_symbol_name) entrypoint_symbol_name = DEFAULT_ENTRYPOINT_SYMBOL_NAME;
    Symbol *symbol = get_defined_symbol(global_symbol_table, entrypoint_symbol_name, 0);
    if (!symbol) error("Missing %s symbol", entrypoint_symbol_name);
    output_elf_file->entrypoint = symbol->dst_value;
}

// Find the virtual address and size of the TLS template, if present.
static void prepare_tls_template(OutputElfFile *output_elf_file) {
    OutputSection *tdata_section = get_output_section(output_elf_file, ".tdata");
    OutputSection *tbss_section = get_output_section(output_elf_file, ".tbss");

    // If there are both .tdata and .bss sections, ensure they are are consecutive
    if (tdata_section && tbss_section && tdata_section->index != tbss_section->index - 1)
        panic(".tdata and .tss sections aren't consecutive: %d != %d - 1", tdata_section->index, tbss_section->index);

    uint64_t align;
    if (tdata_section && !tbss_section) {
        output_elf_file->tls_template_size = tdata_section->size;
        output_elf_file->tls_template_address = tdata_section->address;
        align = tdata_section->align;
    }
    else if (!tdata_section && tbss_section) {
        output_elf_file->tls_template_size = tbss_section->size;
        output_elf_file->tls_template_address = tbss_section->address;
        align = tbss_section->align;
    }
    else if (tdata_section && tbss_section) {
        output_elf_file->tls_template_size = tbss_section->offset - tdata_section->offset + tbss_section->size;
        output_elf_file->tls_template_address = tdata_section->address;
        align = tdata_section->align > tbss_section->align ? tdata_section->align : tbss_section->align;
    }

    // See 3.4.6 of ELF Handling for Thread Local Storage
    // The offset from the thread pointer to the start of the template has to be a multiple of the aligment.
    output_elf_file->tls_template_tls_offset = ALIGN_UP(output_elf_file->tls_template_size, align);
}

// Copy the memory for all program sections in the input files to the output file
static void copy_input_elf_sections_to_output(List *input_elf_files, OutputElfFile *output_elf_file) {
    // Loop over all files
    for (int i = 0; i < input_elf_files->length; i++) {
        InputElfFile *input_elf_file = input_elf_files->elements[i];

        // Loop over all sections
        for (int j = 0; j < input_elf_file->section_list->length; j++) {
            InputSection *input_section = input_elf_file->section_list->elements[j];

            // Skip empty sections
            if (!input_section->size) continue;

            // Only include sections that have program data
            if (!input_section->output_section) continue;

            // Ignore sections without data
            if (input_section->type == SHT_NOBITS) continue;

            const char *section_name = input_section->name;
            OutputSection *rw_section = input_section->output_section;
            if (!rw_section) continue; // The section is not included

            // Allocate memory if not already done in a previous loop
            if (!rw_section->data) rw_section->data = calloc(1, rw_section->size);

            // Load the section data. It may already have been loaded and modified by relocations.
            load_section(input_elf_file, input_section);
            memcpy(rw_section->data + input_section->dst_offset, input_section->data, input_section->size);
        }
    }
}

// Copy the memory for all program sections in the input files to the output file
static void copy_extra_sections_to_output(OutputElfFile *output_elf_file) {
    strmap_ordered_foreach(output_elf_file->extra_sections, it) {
        const char *name = strmap_ordered_iterator_key(&it);
        InputSection *input_section = strmap_ordered_get(output_elf_file->extra_sections, name);

        // Skip empty sections
        if (!input_section->size) continue;

        const char *section_name = input_section->name;
        OutputSection *output_section = input_section->output_section;
        if (!output_section) panic("Extra section %s did not get included in the output", section_name);

        // Allocate memory if not already done
        if (!output_section->data) output_section->data = calloc(1, output_section->size);

        // Copy the data
        if (input_section->dst_offset + input_section->size > output_section->size)
            panic("Attempt to write beyond allocated space of output section %s: %#lx > %#lx",
                output_section->name, input_section->dst_offset + input_section->size, output_section->size);

        memcpy(output_section->data + input_section->dst_offset, input_section->data, input_section->size);
    }
}

OutputElfFile *run(List *library_paths, List *linker_scripts, List *input_files, const char *output_filename, int output_type, char *dynamic_linker, char *soname) {
    // Create output file
    OutputElfFile *output_elf_file = init_output_elf_file(output_filename, output_type);

    // Setup symbol tables
    init_symbols(output_elf_file);

    parse_linker_scripts(output_elf_file, library_paths, linker_scripts);

    // Read input file
    List *input_elf_files = read_input_files(library_paths, input_files, output_type);

    // Add some sections that are always present in output ELF file, e.g. .symtab.
    create_default_sections(output_elf_file);

    // Go through the linker script and set the entrypoint symbol
    set_entrypoint_symbol(output_elf_file, input_elf_files);

    // Run through the first pass of the linker script
    layout_input_sections(output_elf_file, input_elf_files);

    // Run through linker script, group sections into program segments, determine section offsets and assign addresses to symbols in the script
    layout_output_sections(output_elf_file, input_elf_files);

    // Add a _DYNAMIC symbol for ET_DYN outputs
    add_dynamic_symbol(output_elf_file);

    // For all PROVIDE and PROVIDE_HIDDEN symbols, check if there are any undefined symbols that match
    resolve_provided_symbols(output_elf_file);

    // Make set of all global symbols that have relocations
    make_global_symbols_in_use(output_elf_file, input_elf_files);

    // At this point all symbols should be defined. Ensure this is the case.
    finalize_symbols(output_elf_file);

    // When making a shared library, convert all weak symbols to local, with default visibility
    convert_hidden_symbols(output_elf_file);

    // Relax instructions where possible and determine which symbols need to be in the GOT
    process_relocations(output_elf_file, input_elf_files, RELOCATION_PHASE_SCAN);

    // Create the .got, .got.plt, .plt, .rela.plt sections, as needed
    create_got_plt_and_rela_sections(output_elf_file);

    // Create the _GLOBAL_OFFSET_TABLE_ symbol if it's needed
    create_got_symbol(output_elf_file);

    // Process IFUNCs and create .got.plt, .iplt and .rela.iplt if required
    process_ifuncs(output_elf_file, input_elf_files);

    // Allocate memory for extra sections like .got.plt, .iplt and .rela.iplt
    allocate_extra_sections(output_elf_file);

    // Allocate space in the .data.copy extra section for symbols needing the R_X86_64_COPY relocation
    layout_data_copy_section(output_elf_file);

    // For libraries, add the symbols to the ELF dynsym table
    make_elf_dyn_symbols(output_elf_file);

    // Create the .gnu.version section, if needed
    make_versym_section(output_elf_file);

    // Create the .gnu.version_r section, if needed
    make_verneed_section(output_elf_file);

    // For ET_DYN files, allocate space for relocation entries in the GOT table
    create_dyn_rela_section(output_elf_file);

    // Add .dynamic section if it's a shared library
    create_dynamic_sections(output_elf_file, input_elf_files, soname);

    // Add an .interp section if the dynamic linker is set
    create_interp_section(output_elf_file, dynamic_linker);

    // For ET_DYN files, make a hash table of the symbols
    make_symbol_hashes(output_elf_file);

    // Run through the linker script again to include extra sections
    // At this point, input section sizes aren't allowed to change any more
    layout_input_sections(output_elf_file, input_elf_files);

    // If there are any common symbols, create a bss section and allocate values to the symbols
    add_common_symbols_to_bss(output_elf_file);

    // Run through linker script again, determine section offsets and assign addresses to symbols in the script
    // At this point, output section sizes aren't allowed to change any more
    layout_output_sections(output_elf_file, input_elf_files);

    // Update values in .dynamic section
    update_dynamic_sections(output_elf_file, soname);

    // Remove sections from the section list that did not get included in the final file
    remove_empty_sections(output_elf_file);

    // Rearrange sections list, so that special sections such as .symtab are moved to the end.
    make_section_indexes(output_elf_file);

    // Add the symbols to the ELF symbol table. The values and section indexes will be updated later.
    make_elf_symbols(output_elf_file);

    // At this point all section sizes are known. Assign offsets to the builtin sections and make the ELF section headers.
    make_elf_section_headers(output_elf_file);

    // Associate sections using sh_info
    associate_sections(output_elf_file);

    // Now that the size of the ELF file is known, allocate memory for it.
    allocate_elf_output_memory(output_elf_file);

    // Given a list of sections, group them by type and make the list of program segment headers.
    layout_program_segments(output_elf_file);

    // Make the ELF program segment headers. Include the special TLS section if required.
    make_elf_program_segment_headers(output_elf_file, dynamic_linker);

    // Assign final values to all symbols
    make_symbol_values(output_elf_file, input_elf_files);

    // Assign values to symbols in the linker script
    make_output_section_command_assignments_symbol_values(output_elf_file);

    // Set the symbol values in the .got
    update_got_values(output_elf_file);

    // For ET_DYN files, update .plt, .got.plt and .rela.plt sections
    update_dynamic_relocatable_values(output_elf_file);;

    // For ET_DYN files, update the relocation entries in the GOT table
    update_dyn_rela_section(output_elf_file);

    // Make .iplt jmp instructions that refer to the entries in .got.iplt, also finish off .rela.iplt
    update_iplt(output_elf_file);

    // Set the _GLOBAL_OFFSET_TABLE_ symbol to the address of the .got or .got.plt section
    set_got_symbol_value(output_elf_file);

    // Set the symbol's value and section indexes
    update_elf_symbols(output_elf_file);

    // Set the executable entrypoint
    set_entrypoint(output_elf_file);

    // Check that sections have an increasing offset and don't overlap.
    check_output_sections(output_elf_file);

    // Make the ELF headers and copy the program segment and section headers
    make_elf_headers(output_elf_file);

    // Find the virtual address and size of the TLS template, if present.
    prepare_tls_template(output_elf_file);

    // Copy the memory for all program sections in the input files to the output file
    copy_input_elf_sections_to_output(input_elf_files, output_elf_file);

    // Copy in the extra sections to the output
    copy_extra_sections_to_output(output_elf_file);

    // Write relocated symbol values to the output ELF file
    process_relocations(output_elf_file, input_elf_files, RELOCATION_PHASE_APPLY);

    // Write the ELF file
    write_elf_file(output_elf_file);

    return output_elf_file;
}
