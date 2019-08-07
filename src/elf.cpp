#include "elf.h"

#include <algorithm>

namespace smldbg::elf {

ELF::ELF(std::unique_ptr<std::istream> is) : m_is(std::move(is)) {
    read_file_header();
    read_program_header();
    read_section_headers();
}

ELFSection ELF::get_section_data(std::string section_name) {
    // Check if the data for this section is already in our cache.
    if (m_section_data_cache.count(section_name)) {
        auto& bytes = m_section_data_cache.at(section_name);
        return {.data = bytes.data(), .size = bytes.size()};
    }

    // This is the first time this section has been requested.
    auto& section_data = m_section_data_cache[section_name];
    read_section_data(section_name, section_data);
    return {.data = section_data.data(), .size = section_data.size()};
}

void ELF::read_file_header() {
    m_is->read(reinterpret_cast<char*>(&m_file_header), sizeof(ELFFileHeader));
}

void ELF::read_program_header() {
    m_is->read(reinterpret_cast<char*>(&m_program_header),
               sizeof(ELFProgramHeader));
}

void ELF::read_section_headers() {
    // Seek to the start of the section headers.
    m_is->seekg(m_file_header.E_SHOFF, std::ios::beg);
    m_section_headers.resize(m_file_header.E_SHNUM);
    for (unsigned i = 0, e = m_file_header.E_SHNUM; i < e; ++i) {
        m_is->read(reinterpret_cast<char*>(&m_section_headers[i]),
                   sizeof(ELFSectionHeader));
    }

    // String table offset and size.
    const auto& shstrtab_header = m_section_headers[m_file_header.E_SHSTRNDX];
    const auto shstrtab_offset = shstrtab_header.SH_OFFSET;
    const auto shstrtab_size = shstrtab_header.SH_SIZE;

    // Read the .shstrtab section bytes.
    m_string_table.resize(shstrtab_size);
    m_is->seekg(shstrtab_offset, std::ios::beg);
    m_is->read(reinterpret_cast<char*>(m_string_table.data()), shstrtab_size);

    m_section_header_names.resize(m_file_header.E_SHNUM);
    for (unsigned i = 0, e = m_file_header.E_SHNUM; i < e; ++i) {
        // Find the first null terminator after the name offset.
        const auto null_terminator =
            std::find(m_string_table.begin() + m_section_headers[i].SH_NAME,
                      m_string_table.end(), '\0');

        // Copy the bytes to |m_section_header_names|.
        m_section_header_names[i] =
            std::string(m_string_table.begin() + m_section_headers[i].SH_NAME,
                        null_terminator);
    }
}

bool ELF::read_section_data(std::string_view section_name,
                            std::vector<char>& section_bytes) {
    // Try and find the named section.
    const auto found = std::find(m_section_header_names.begin(),
                                 m_section_header_names.end(), section_name);

    // No section header with this name.
    if (found == m_section_header_names.end())
        return false;

    // Get the header for the section.
    const auto& header = [&]() {
        const auto index = std::distance(m_section_header_names.begin(), found);
        return m_section_headers[index];
    }();

    // Read the section bytes.
    section_bytes.resize(header.SH_SIZE);
    m_is->seekg(header.SH_OFFSET, std::ios::beg);
    m_is->read(reinterpret_cast<char*>(section_bytes.data()), header.SH_SIZE);

    return true;
}

} // namespace smldbg::elf
