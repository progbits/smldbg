#include "compile_unit.h"

namespace smldbg::dwarf {

CompileUnit::CompileUnit(char** debug_info, char* debug_abbrev)
    : m_debug_info(*debug_info), m_debug_abbrev(debug_abbrev) {
    // Section 7.5
    // http://www.dwarfstd.org/doc/DWARF4.pdf
    char* start = *debug_info;
    uint32_t maybe_unit_length =
        smldbg::util::read_bytes<uint32_t>(*debug_info);
    if (maybe_unit_length == 0xFFFFFFFF) {
        is_64bit = true;
        unit_length = smldbg::util::read_bytes<uint64_t>(*debug_info);
    } else {
        is_64bit = false;
        unit_length = maybe_unit_length;
    }
    version = smldbg::util::read_bytes<uint16_t>(*debug_info);
    if (is_64bit)
        debug_abbrev_offset = smldbg::util::read_bytes<uint64_t>(*debug_info);
    else
        debug_abbrev_offset = smldbg::util::read_bytes<uint32_t>(*debug_info);
    address_size = smldbg::util::read_bytes<uint8_t>(*debug_info);

    // Advance |debug_info| to the end of this compile unit.
    std::advance(start, unit_length + (is_64bit ? 12 : 4));
    *debug_info = start;
}

DIE CompileUnit::root() const {
    uint64_t header_size = is_64bit ? (12 + 2 + 8 + 1) : (4 + 2 + 4 + 1);
    return DIE(m_debug_info + header_size,
               m_debug_info + unit_length + (is_64bit ? 12 : 4),
               m_debug_abbrev + debug_abbrev_offset, is_64bit);
}

bool CompileUnit::contains_address(uint64_t address, char* debug_ranges) {
    // Our root DIE should be DW_TAG_compile_unit which should contain a
    // DW_AT_low_pc and either DW_AT_high_pc or DW_AT_ranges attributes.
    DIE die = root();
    if (die.tag() != DW_TAG::DW_TAG_compile_unit) {
        std::cerr << "Root DIE of CompileUnit is not DW_TAG_compile_unit.\n";
        std::exit(1);
    }

    // Check if we have a simple low_pc/high_pc pair.
    if (auto low_pc = die.attribute(DW_AT::DW_AT_low_pc),
        high_pc = die.attribute(DW_AT::DW_AT_high_pc);
        low_pc && high_pc) {
        return address > low_pc->as_uint64t() &&
               address < high_pc->as_uint64t();
    }

    // No low_pc/high_pc pair, must have a set of non-contiguous address ranges.
    std::optional<Attribute> ranges_offset = die.attribute(DW_AT::DW_AT_ranges);
    if (!ranges_offset) {
        std::cerr << "Missing DW_AT_ranges.\n";
        return false;
    }

    // Decode range entries. Stop if we find an interval containing |address|.
    char* debug_ranges_iter = debug_ranges + ranges_offset->as_uint64t();
    uint64_t range_start = util::read_bytes<uint64_t>(debug_ranges_iter);
    uint64_t range_end = util::read_bytes<uint64_t>(debug_ranges_iter);
    while (range_start && range_end) {
        if (address >= range_start && address <= range_end)
            return true;
        range_start = util::read_bytes<uint64_t>(debug_ranges_iter);
        range_end = util::read_bytes<uint64_t>(debug_ranges_iter);
    }

    return false;
}

} // namespace smldbg::dwarf