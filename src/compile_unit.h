#pragma once

#include "die.h"
#include "util.h"

#include <cstdint>

namespace smldbg::dwarf {

class CompileUnit {

public:
    // Construct a new CompileUnit instance from a .debug_info entry.
    //
    // Preconditions: |debug_info| should point to the first byte of the
    // .debug_info entry for the compile unit.
    //
    // Postconditions : |debug_info| is advanced by the size of the compile
    // unit. I.e. after construction |debug_info| points to the first byte of
    // the next compile unit.
    CompileUnit(char** debug_info, char* debug_abbrev);

    // Return the root (first) Debug Information Entry (DIE) for the compile
    // unit (normally DW_TAG_compile_unit). The DIE instance can be used to
    // iterate the other tags of this compile unit and extract attribute values.
    DIE root() const;

    // Does |address| fall into the range of addresses represented by the
    // compile unit?
    //
    // Precondition: |debug_ranges| should point to the start of the
    // .debug_ranges section of the corresponding ELF file.
    //
    // Postconditions: None.
    bool contains_address(uint64_t address, char* debug_ranges);

private:
    bool is_64bit;
    uint64_t unit_length;
    uint16_t version;
    uint64_t debug_abbrev_offset;
    uint8_t address_size;

    char* m_debug_info; // Points to the first byte of the .debug_info entry for
                        // the compile unit.

    char* m_debug_abbrev; // Points to the first byte of the .debug_abbrev
                          // section of the parent ELF file. The start of
                          // the debug_abbrev entry for this compile
                          // unit is located at |m_debug_abbrev| +
                          // |debug_abbrev_offset|.
};

} // namespace smldbg::dwarf
