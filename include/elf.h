#ifndef _ELF_H
#define _ELF_H

#include <stdint.h>

#include "strmap.h"

// https://en.wikipedia.org/wiki/Executable_and_Linkable_Format
// https://github.com/lattera/glibc/blob/master/elf/elf.h
// https://github.com/torvalds/linux/blob/master/include/uapi/linux/elf.h
// https://android.googlesource.com/platform/bionic/+/master/libc/include/elf.h

// ELF class
#define ELF_CLASS_NONE    0   // Invalid class
#define ELF_CLASS_32      1   // 32-bit objects
#define ELF_CLASS_64      2   // 64-bit objects

// ELf endianness
#define ELF_DATA_2_LSB	  1   // 2's complement, little endian
#define ELF_DATA_2_MSB	  2   // 2's complement, big endian

// ELF ABI
#define ELF_OSABI_NONE    0	 // UNIX System V ABI
#define ELF_OSABI_HPUX    1	 // HP-UX
#define ELF_OSABI_NETBSD  2	 // NetBSD
#define ELF_OSABI_GNU     3	 // Object uses GNU ELF extensions


// Legal values for e_version (version).
#define EV_NONE		0               // Invalid ELF version
#define EV_CURRENT	1               // Current version
#define EV_NUM		2

#define SHN_UNDEF         0         // This section table index means the symbol is undefined. When the link editor combines this object file with another that defines the indicated symbol, this file's references to the symbol will be linked to the actual definition.
#define SHN_ABS           65521     // The symbol has an absolute value that will not change because of relocation.#define
#define SHN_COMMON        65522     // This symbol labels a common block that has not yet been allocate

#define SHF_WRITE         (1 << 0)  // Writeable
#define SHF_ALLOC         (1 << 1)  // Occupies memory
#define SHF_EXECINSTR     (1 << 2)  // Executable
#define SHF_MERGE         (1 << 4)  // Could be merged
#define SHF_STRINGS       (1 << 5)  // Contains strings
#define SHF_INFO_LINK     (1 << 6)  // sh_info contains SHT index
#define SHF_GROUP         (1 << 9)	// Section is member of a group
#define SHF_TLS           (1 << 10)	// TLS

// Section header types
#define SHT_NULL           0    // Unused
#define SHT_PROGBITS       1    // Program data
#define SHT_SYMTAB         2    // Symbol table
#define SHT_STRTAB         3    // String table
#define SHT_RELA           4    // Relocation entries with addends
#define SHT_HASH           5    // Symbol hash table
#define SHT_DYNAMIC        6    // Dynamic linking information
#define SHT_NOTE	       7    // Notes
#define SHT_NOBITS         8    // Program space with no data (bss)
#define SHT_REL            9    // Relocation entries, no addends
#define SHT_SHLIB         10    // Reserved
#define SHT_DYNSYM        11    // Dynamic linker symbol table
#define SHT_INIT_ARRAY    14    // Array of constructors
#define SHT_FINI_ARRAY    15    // Array of destructors
#define SHT_PREINIT_ARRAY 16    // Array of pre-constructors
#define SHT_GROUP         17    // Section group
#define SHT_SYMTAB_SHNDX  18    // Extended section indeces
#define	SHT_NUM           19    // Number of defined types.

// Program segment types
#define	PT_NULL           0     // Program header table entry unused
#define PT_LOAD           1     // Loadable program segment
#define PT_DYNAMIC        2     // Dynamic linking information
#define PT_INTERP         3     // Program interpreter
#define PT_NOTE           4     // Auxiliary information
#define PT_SHLIB          5     // Reserved
#define PT_PHDR           6     // Entry for header table itself
#define PT_TLS            7     // Thread-local storage segment
#define	PT_NUM            8     // Number of defined types

// Program segment flags
#define PF_X            0x01 // Segment is executable
#define PF_W            0x02 // Segment is writable
#define PF_R            0x04 // Segment is readable

// Symbol bindings
#define STB_LOCAL       0
#define STB_GLOBAL      1
#define STB_WEAK        2

// Symbol types
#define STT_NOTYPE      0
#define STT_OBJECT      1
#define STT_FUNC        2
#define STT_SECTION     3
#define STT_FILE        4
#define STT_COMMON      5
#define STT_TLS         6
#define STT_LOOS        10
#define STT_GNU_IFUNC   10      // Symbol is an indirect code object
#define STT_HIOS        12
#define STT_LOPROC      13
#define STT_HIPROC      15

// See http://refspecs.linuxbase.org/elf/x86_64-abi-0.98.pdf page 69
// A Represents the addend used to compute the value of the relocatable field
// G Represents the offset into the global offset table at which the relocation entry’s symbol will reside during execution.
// L Represents the place (section offset or address) of the Procedure Linkage Table entry for a symbol.
// P Represents the place (section offset or address) of the storage unit being relocated (computed using r_offset).
// T is the start of the TLS template
// https://android.googlesource.com/platform/prebuilts/gcc/linux-x86/host/x86_64-linux-glibc2.7-4.6/+/02075080d51c371ae87b9898bf84a085e436ee27/sysroot/usr/include/elf.h#2526
#define R_X86_64_NONE            0   // No calculation
#define R_X86_64_64              1   // Direct 64 bit                 S + A
#define R_X86_64_PC32            2   // PC relative 32 bit signed     S + A - P
#define R_X86_64_GOT32           3   // 32 bit GOT entry              G + A
#define R_X86_64_PLT32           4   // 32 bit PLT address            L + A - P
#define R_X86_64_GOTPCREL        9	 // 32 bit signed PC relative offset to GOT
#define R_X86_64_32             10   // Direct 8 bit                  S + A
#define R_X86_64_32S            11   // Direct 8 bit sign extended    S + A
#define R_X86_64_16             12   // Direct 8 bit                  S + A
#define R_X86_64_16S            13   // Direct 8 bit sign extended    S + A
#define R_X86_64_8              14   // Direct 8 bit                  S + A
#define R_X86_64_GOTTPOFF       22	 // 32 bit signed PC relative offset to GOT entry for IE symbol
#define R_X86_64_TPOFF32        23   // Offset in initial TLS block   S + A - T
#define R_X86_64_IRELATIVE      37   // Adjust indirectly by program base
#define R_X86_64_GOTPCRELX      41   // Introduced in GNU binutils 2.26
#define R_X86_64_REX_GOTPCRELX  42   // Introduced in GNU binutils 2.26
#define E_MACHINE_TYPE_X86_64   0x3e

// Object file type
#define ET_NONE  0 // No file type
#define ET_REL   1 // relocatable
#define ET_EXEC  2 // Executable file
#define ET_DYN   3 // Shared object file

// Symbol visibility specification encoded in the st_other field
#define STV_DEFAULT     0 // Default symbol visibility rules
#define STV_INTERNAL    1 // Processor specific hidden class
#define STV_HIDDEN      2 // Sym unavailable in other modules
#define STV_PROTECTED   3 // Not preemptible, not exported

// Dynamic section tags
#define DT_NULL            0    // Marks end of dynamic section
#define DT_NEEDED          1    // Name of needed library
#define DT_PLTRELSZ        2    // Size in bytes of PLT relocs
#define DT_PLTGOT          3    // Processor defined value
#define DT_HASH            4    // Address of symbol hash table
#define DT_STRTAB          5    // Address of string table
#define DT_SYMTAB          6    // Address of symbol table
#define DT_RELA            7    // Address of Rela relocs
#define DT_RELASZ          8    // Total size of Rela relocs
#define DT_RELAENT         9    // Size of one Rela reloc
#define DT_STRSZ           10   // Size of string table
#define DT_SYMENT          11   // Size of one symbol table entry
#define DT_INIT            12   // Address of init function
#define DT_FINI            13   // Address of termination function
#define DT_SONAME          14   // Name of shared object
#define DT_RPATH           15   // Library search path (deprecated)
#define DT_SYMBOLIC        16   // Start symbol search here
#define DT_REL             17   // Address of Rel relocs
#define DT_RELSZ           18   // Total size of Rel relocs
#define DT_RELENT          19   // Size of one Rel reloc
#define DT_PLTREL          20   // Type of reloc in PLT
#define DT_DEBUG           21   // For debugging; unspecified
#define DT_TEXTREL         22   // Reloc might modify .text
#define DT_JMPREL          23   // Address of PLT relocs
#define DT_BIND_NOW        24   // Process relocations of object
#define DT_INIT_ARRAY      25   // Array with addresses of init fct
#define DT_FINI_ARRAY      26   // Array with addresses of fini fct
#define DT_INIT_ARRAYSZ    27   // Size in bytes of DT_INIT_ARRAY
#define DT_FINI_ARRAYSZ    28   // Size in bytes of DT_FINI_ARRAY
#define DT_RUNPATH         29   // Library search path
#define DT_FLAGS           30   // Flags for the object being loaded
#define DT_ENCODING        32   // Start of encoded range
#define DT_PREINIT_ARRAY   32   // Array with addresses of preinit fct
#define DT_PREINIT_ARRAYSZ 33   // size in bytes of DT_PREINIT_ARRAY
#define DT_SYMTAB_SHNDX    34   // Address of SYMTAB_SHNDX section

typedef struct elf_header {
    uint8_t   ei_magic0;        // 0x7F followed by ELF(45 4c 46) in ASCII; these four bytes constitute the magic number.
    uint8_t   ei_magic1;
    uint8_t   ei_magic2;
    uint8_t   ei_magic3;
    uint8_t   ei_class;         // This byte is set to either 1 or 2 to signify 32- or 64-bit format, respectively.
    uint8_t   ei_data;          // This byte is set to either 1 or 2 to signify little or big endianness, respectively.
    uint8_t   ei_version;       // Set to 1 for the original version of ELF.
    uint8_t   ei_osabi;         // Identifies the target operating system ABI.
    uint8_t   ei_osabiversion;  // Further specifies the ABI version.
    uint8_t   pad0;             // Unused
    uint8_t   pad1;             // Unused
    uint8_t   pad2;             // Unused
    uint8_t   pad3;             // Unused
    uint8_t   pad4;             // Unused
    uint8_t   pad5;             // Unused
    uint8_t   pad6;             // Unused
    uint16_t  e_type;           // File type.
    uint16_t  e_machine;        // Machine architecture.
    uint32_t  e_version;        // ELF format version.
    uint64_t  e_entry;          // Entry point.
    uint64_t  e_phoff;          // Program header file offset.
    uint64_t  e_shoff;          // Section header file offset.
    uint32_t  e_flags;          // Architecture-specific flags.
    uint16_t  e_ehsize;         // Size of ELF header in bytes.
    uint16_t  e_phentsize;      // Size of program header entry.
    uint16_t  e_phnum;          // Number of program header entries.
    uint16_t  e_shentsize;      // Size of section header entry.
    uint16_t  e_shnum;          // Number of section header entries.
    uint16_t  e_shstrndx;       // Section name strings section.
} ElfHeader;

typedef struct elf_section_header {
    uint32_t sh_name;           // An offset to a string in the .shstrtab section that represents the name of this section
    uint32_t sh_type;           // Identifies the type of this header.
    uint64_t sh_flags;          // Identifies the attributes of the section.
    uint64_t sh_addr;           // Virtual address of the section in memory, for sections that are loaded.
    uint64_t sh_offset;         // Offset of the section in the file image.
    uint64_t sh_size;           // Size in bytes of the section in the file image. May be 0.
    uint32_t sh_link;           // Contains the section index of an associated section.
    uint32_t sh_info;           // Contains extra information about the section.
    uint64_t sh_addralign;      // Contains the required alignment of the section. This field must be a power of two.
    uint64_t sh_entsize;        // Contains the size, in bytes, of each entry, for sections that contain fixed-size entries. Otherwise, this field contains zero.
} ElfSectionHeader;

typedef struct elf_program_segment_header{
    uint32_t p_type;            // Segment type
    uint32_t p_flags;           // Segment flags
    uint64_t p_offset;          // Segment file offset
    uint64_t p_vaddr;           // Segment virtual address
    uint64_t p_paddr;           // Segment physical address
    uint64_t p_filesz;          // Segment size in file
    uint64_t p_memsz;           // Segment size in memory
    uint64_t p_align;           // Segment alignment
} ElfProgramSegmentHeader;

typedef struct elf_symbol {
    uint32_t st_name;           // This member holds an index into the object file's symbol string table
    uint8_t  st_info;           // This member specifies the symbol's type (low 4 bits) and binding (high 4 bits) attributes
    uint8_t  st_other;          // This member currently specifies a symbol's visibility.
    uint16_t st_shndx;          // Every symbol table entry is defined in relation to some section. This member holds the relevant section header table index.
    uint64_t st_value;          // This member gives the value of the associated symbol. Depending on the context, this may be an absolute value, an address, and so on; details appear
    uint64_t st_size;           // Many symbols have associated sizes. For example, a data object's size is the number of bytes contained in the object. This member holds 0 if the symbol has no size or an unknown size.
} ElfSymbol;

typedef struct elf_relocation {
    uint64_t r_offset;          // Location to be relocated
    uint64_t r_info;            // Relocation type (low 32 bits) and symbol index (high 32 bits).
    uint64_t r_addend;          // Addend
} ElfRelocation;

typedef struct elf_dyn {
    int64_t	d_tag;      // Dynamic entry type
    union {
        uint64_t d_val; // Integer value
        uint64_t d_ptr; // Address value
    } d_un;
} ElfDyn;

#endif
