#pragma once

#include <cstdint>
#include <istream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace smldbg::elf {

// 64 bit ELF file header.
#pragma(pack(1))
struct ELFFileHeader {
    uint32_t EI_MAG;
    uint8_t EI_CLASS;
    uint8_t EI_DATA;
    uint8_t EI_VERSION;
    uint8_t EI_OSABI;
    uint64_t EI_ABIVERSION;
    uint16_t E_TYPE;
    uint16_t E_MACHINE;
    uint32_t E_VERSION;
    uint64_t E_ENTRY;
    uint64_t E_PHOFF;
    uint64_t E_SHOFF;
    uint32_t E_FLAGS;
    uint16_t E_EHSIZE;
    uint16_t E_PHENTSIZE;
    uint16_t E_PHNUM;
    uint16_t E_SHENTSIZE;
    uint16_t E_SHNUM;
    uint16_t E_SHSTRNDX;
};

// 64 bit ELF program header.
#pragma(pack(1))
struct ELFProgramHeader {
    uint32_t P_TYPE;
    uint32_t P_FLAGS;
    uint64_t P_OFFSET;
    uint64_t P_VADDR;
    uint64_t P_PADDR;
    uint64_t P_FILESZ;
    uint64_t P_MEMSZ;
    uint64_t P_ALIGN;
};

// 64 bit ELF section header.
#pragma(pack(1))
struct ELFSectionHeader {
    uint32_t SH_NAME;
    uint32_t SH_TYPE;
    uint64_t SH_FLAGS;
    uint64_t SH_ADDR;
    uint64_t SH_OFFSET;
    uint64_t SH_SIZE;
    uint32_t SH_LINK;
    uint32_t SH_INFO;
    uint64_t SH_ADDRALIGN;
    uint64_t SH_ENTSIZE;
};

struct ELFSection {
    char* data;
    uint64_t size;
};

class ELF {
public:
    ELF() = default;

    // Construct a new ELF instance from a stream.
    //
    // Preconditions: |is| should represent the byte stream of an ELF format
    // file.
    ELF(std::unique_ptr<std::istream> is);

    ELFSection get_section_data(std::string section_name);

private:
    void read_file_header();
    void read_program_header();
    void read_section_headers();
    bool read_section_data(std::string_view section_name,
                           std::vector<char>& section_bytes);

    std::unique_ptr<std::istream> m_is;

    ELFFileHeader m_file_header;
    ELFProgramHeader m_program_header;
    std::vector<ELFSectionHeader> m_section_headers;

    std::vector<char> m_string_table;
    std::vector<std::string> m_section_header_names;
    std::unordered_map<std::string, std::vector<char>> m_section_data_cache;
};

} // namespace smldbg::elf
