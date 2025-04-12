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

#define SHN_UNDEF         0        // This section table index means the symbol is undefined. When the link editor combines this object file with another that defines the indicated symbol, this file's references to the symbol will be linked to the actual definition.
#define SHN_ABS           65521    // The symbol has an absolute value that will not change because of relocation.#define
#define SHN_COMMON        65522    // This symbol labels a common block that has not yet been allocate
#define SHF_WRITE         0x01
#define SHF_ALLOC         0x02
#define SHF_EXECINSTR     0x04
#define SHF_MERGE         0x10
#define SHF_STRINGS       0x20

// Section header types
#define SHT_NULL        0x00    // Unused
#define SHT_PROGBITS    0x01    // Program data
#define SHT_SYMTAB      0x02    // Symbol table
#define SHT_STRTAB      0x03    // String table
#define SHT_RELA        0x04    // Relocation entries with addends
#define SHT_NOTE	    0x07    // Notes
#define SHT_NOBITS      0x08    // Program space with no data (bss)
#define SHF_INFO_LINK   0x40

// Program segment types
#define	PT_NULL		    0       // Program header table entry unused
#define PT_LOAD		    1       // Loadable program segment

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
#define STT_HIOS        12
#define STT_LOPROC      13
#define STT_HIPROC      15

// See http://refspecs.linuxbase.org/elf/x86_64-abi-0.98.pdf page 69
// A Represents the addend used to compute the value of the relocatable field
// G Represents the offset into the global offset table at which the relocation entry’s symbol will reside during execution.
// L Represents the place (section offset or address) of the Procedure Linkage Table entry for a symbol.
// P Represents the place (section offset or address) of the storage unit being relocated (computed using r_offset).
// https://android.googlesource.com/platform/prebuilts/gcc/linux-x86/host/x86_64-linux-glibc2.7-4.6/+/02075080d51c371ae87b9898bf84a085e436ee27/sysroot/usr/include/elf.h#2526
#define R_X86_64_NONE            0   // No calculation
#define R_X86_64_64              1   // Direct 64 bit                 S + A
#define R_X86_64_PC32            2   // PC relative 32 bit signed     S + A - P
#define R_X86_64_GOT32           3   // 32 bit GOT entry              G + A
#define R_X86_64_PLT32           4   // 32 bit PLT address            L + A - P
#define R_X86_64_32             10   // Direct 8 bit                  S + A
#define R_X86_64_32S            11   // Direct 8 bit sign extended    S + A
#define R_X86_64_16             12   // Direct 8 bit                  S + A
#define R_X86_64_16S            13   // Direct 8 bit sign extended    S + A
#define R_X86_64_8              14   // Direct 8 bit                  S + A
#define R_X86_64_REX_GOTP       42   // Introduced in GNU binutils 2.26

#define E_MACHINE_TYPE_X86_64   0x3e

// Object file type
#define ET_NONE  0 // No file type
#define ET_REL   1 // relocatable
#define ET_EXEC  2 // Executable file
#define ET_DYN   3 // Shared object file

// Symbol visibility specification encoded in the st_other field
#define STV_DEFAULT     0 / Default symbol visibility rules
#define STV_INTERNAL    1 / Processor specific hidden class
#define STV_HIDDEN      2 / Sym unavailable in other modules
#define STV_PROTECTED   3 / Not preemptible, not exported

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

#endif
