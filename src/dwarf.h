#pragma once

#include "attribute.h"
#include "compile_unit.h"
#include "die.h"
#include "elf.h"

#include <cstdint>
#include <cstring>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace smldbg::dwarf {

struct SourceLocation {
    uint64_t address;      // Program counter value for the source location.
    uint64_t line;         // Source line.
    std::string_view file; // Source file.
    bool is_stmt;          // Is the source location tagged as a statement?
    bool prologue_end; // Is the source location tagged as the end of a function
                       // prologue?
};

struct VariableLocation {
    uint64_t address;
    uint64_t register_id;
};

class Dwarf {
public:
    Dwarf() = default;
    Dwarf(elf::ELF* elf);

    // Return the source location of the named function.
    std::optional<SourceLocation>
    source_location_from_function(std::string_view function);

    // Return the program counter value associated with |line| of |file|.
    std::optional<uint64_t>
    program_counter_from_line_and_file(uint64_t line, std::string_view file);

    // Return the source location associated with a program counter value.
    std::optional<SourceLocation>
    source_location_from_program_counter(uint64_t program_counter,
                                         bool skip_prologues);

    // Return the name of the function associated with a program counter value.
    std::optional<std::string>
    function_from_program_counter(uint64_t program_counter);

    // Return the location of a variable at a specific program counter value.
    std::optional<int64_t> variable_location(uint64_t program_counter,
                                             std::string_view variable_name);

private:
    // Read each of the compile units present in |m_elf|.
    void read_compile_units();

    // Return debug information entries with a tag matching |tag|.
    std::vector<DIE> filter_die_by_tag(DW_TAG tag);

    // Get the offset into the .debug_line sections for the compile unit
    // representing |file|.
    std::optional<uint64_t> debug_line_offset_from_file(std::string_view file);

    elf::ELF* m_elf; // The ELF file of the debug target.

    std::vector<CompileUnit>
        m_compile_units; // The compileu nits present in the .debug_info
                         // section of |m_elf|.
};

} // namespace smldbg::dwarf
