#include "dwarf.h"

#include "compile_unit.h"
#include "dwarf_location_stack_machine.h"
#include "elf.h"
#include "line_vm.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace smldbg::dwarf {

using namespace util;

Dwarf::Dwarf(elf::ELF* elf) : m_elf(elf) { read_compile_units(); }

std::optional<SourceLocation>
Dwarf::source_location_from_function(std::string_view function) {
    elf::ELFSection debug_str = m_elf->get_section_data(".debug_str");

    std::optional<uint64_t> address;
    std::vector<DIE> entries = filter_die_by_tag(DW_TAG::DW_TAG_subprogram);
    for (auto& entry : entries) {
        if (auto name = entry.attribute(DW_AT::DW_AT_name); name) {
            if (name->as_string_view(debug_str.data) != function)
                continue;
            if (auto low_pc = entry.attribute(DW_AT::DW_AT_low_pc); low_pc)
                address = low_pc->as_uint64t();
        }
        if (address)
            break;
    }

    if (!address)
        return std::nullopt;

    return source_location_from_program_counter(*address, true);
}

std::optional<uint64_t>
Dwarf::program_counter_from_line_and_file(uint64_t line,
                                          std::string_view file) {
    // Get the .debug_line section offset for |file|.
    const auto offset = debug_line_offset_from_file(file);
    if (!offset)
        return std::nullopt;

    // Run the line number virtual machine to generate the line number table.
    elf::ELFSection debug_line = m_elf->get_section_data(".debug_line");
    elf::ELFSection debug_str = m_elf->get_section_data(".debug_str");
    LineVM vm(debug_line.data + *offset, debug_str.data);
    vm.exec();

    // Find the closest line to |line| in |file|, favoring statements.
    std::vector<LineNumberTableRow> line_numbers = vm.table();
    int best_match = std::numeric_limits<int>::max();
    int min_distance = std::numeric_limits<int>::max();
    for (unsigned i = 0, e = line_numbers.size(); i < e; ++i) {
        if (line_numbers[i].file != file)
            continue;
        int candidate = line_numbers[i].line;
        const auto distance = std::abs(static_cast<int>(line) - candidate);
        if (distance < min_distance && line_numbers[i].is_stmt) {
            best_match = i;
            min_distance = distance;
        }
    }

    // Didn't find a match.
    if (best_match == std::numeric_limits<unsigned>::max())
        return std::nullopt;

    // If we can, skip function prologues.
    if (best_match < line_numbers.size() - 1 &&
        line_numbers[best_match + 1].prologue_end)
        ++best_match;

    return line_numbers[best_match].address;
}

std::optional<SourceLocation>
Dwarf::source_location_from_program_counter(uint64_t program_counter,
                                            bool skip_prologues) {
    // Find the compile unit that contains |program_counter|.
    const CompileUnit* compile_unit = [=]() -> CompileUnit* {
        elf::ELFSection debug_ranges = m_elf->get_section_data(".debug_ranges");
        for (auto& cu : m_compile_units) {
            if (cu.contains_address(program_counter, debug_ranges.data))
                return &cu;
        }
        return nullptr;
    }();

    if (!compile_unit)
        return std::nullopt;

    // Get the offset into .debug_line.
    std::optional<Attribute> stmt_list =
        compile_unit->root().attribute(DW_AT::DW_AT_stmt_list);
    if (!stmt_list)
        return std::nullopt;
    const uint64_t offset = stmt_list->as_uint64t();

    // Run the line number virtual machine to generate the line number table.
    elf::ELFSection debug_line = m_elf->get_section_data(".debug_line");
    elf::ELFSection debug_str = m_elf->get_section_data(".debug_str");
    LineVM vm(debug_line.data + offset, debug_str.data);
    vm.exec();

    // Find the line number entry which best matches |program_counter|.
    std::vector<LineNumberTableRow> line_numbers = vm.table();
    int best_match = std::numeric_limits<int>::max();
    for (unsigned i = 1, e = line_numbers.size(); i < e; ++i) {
        const auto previous_address = line_numbers[i - 1].address;
        const auto current_address = line_numbers[i].address;
        if (previous_address <= program_counter &&
            current_address > program_counter) {
            if (line_numbers[i - 1].end_sequence)
                continue;
            best_match = i - 1;
        }
    }

    if (best_match == std::numeric_limits<unsigned>::max())
        return std::nullopt;

    // If we can, skip function prologues.
    if (skip_prologues && best_match < line_numbers.size() - 1 &&
        line_numbers[best_match + 1].prologue_end)
        ++best_match;

    return SourceLocation{.address = line_numbers[best_match].address,
                          .line = line_numbers[best_match].line,
                          .file = line_numbers[best_match].file,
                          .is_stmt = line_numbers[best_match].is_stmt,
                          .prologue_end =
                              line_numbers[best_match].prologue_end};
}

std::optional<std::string>
Dwarf::function_from_program_counter(uint64_t program_counter) {
    std::optional<DIE> subprogram;
    std::vector<DIE> entries = filter_die_by_tag(DW_TAG::DW_TAG_subprogram);
    for (unsigned i = 0, e = entries.size(); i < e && !subprogram; ++i) {
        auto low_pc_atr = entries[i].attribute(DW_AT::DW_AT_low_pc);
        auto high_pc_atr = entries[i].attribute(DW_AT::DW_AT_high_pc);
        if (!low_pc_atr || !high_pc_atr)
            continue;

        // high_pc is either an absolute address or an offset.
        const uint64_t low_pc = low_pc_atr->as_uint64t();
        const uint64_t high_pc = [&]() -> uint64_t {
            if (high_pc_atr->form() == DW_FORM::DW_FORM_addr)
                return high_pc_atr->as_uint64t();
            return low_pc + high_pc_atr->as_uint64t();
        }();

        if (low_pc <= program_counter && program_counter <= high_pc)
            subprogram = entries[i];
    }

    if (!subprogram)
        return std::nullopt;

    // Check if the subprogram has a name (or linkage_name).
    auto name = [&]() {
        auto name = subprogram->attribute(DW_AT::DW_AT_name);
        auto linkage_name = subprogram->attribute(DW_AT::DW_AT_linkage_name);
        return name ? name : linkage_name ? linkage_name : std::nullopt;
    }();

    if (!name)
        return std::nullopt;

    elf::ELFSection debug_str = m_elf->get_section_data(".debug_str");
    return std::string(name->as_string_view(debug_str.data));
}

std::optional<int64_t>
Dwarf::variable_location(uint64_t program_counter,
                         std::string_view variable_name) {
    // Find the subprogram associated with |program_counter|.
    std::optional<DIE> subprogram;
    std::vector<DIE> entries = filter_die_by_tag(DW_TAG::DW_TAG_subprogram);
    for (unsigned i = 0, e = entries.size(); i < e && !subprogram; ++i) {
        auto low_pc = entries[i].attribute(DW_AT::DW_AT_low_pc);
        auto high_pc = entries[i].attribute(DW_AT::DW_AT_high_pc);
        if (!low_pc || !high_pc)
            continue;

        // |high_pc| is either an absolute address or an offset from |low_pc|.
        const uint64_t low = low_pc->as_uint64t();
        const uint64_t high = [&]() -> uint64_t {
            if (high_pc->form() == DW_FORM::DW_FORM_addr)
                return high_pc->as_uint64t();
            return low + high_pc->as_uint64t();
        }();

        if (low <= program_counter && program_counter <= high)
            subprogram = entries[i];
    }

    if (!subprogram)
        return std::nullopt;

    // Get the location of the variable.
    std::optional<Attribute> location;
    std::vector<DIE> nested = subprogram->get_nested();
    for (unsigned i = 0, e = nested.size(); i < e && !location; ++i) {
        elf::ELFSection debug_str = m_elf->get_section_data(".debug_str");
        if (auto name = nested[i].attribute(DW_AT::DW_AT_name); name)
            if (name->as_string_view(debug_str.data) != variable_name)
                continue;
        location = nested[i].attribute(DW_AT::DW_AT_location);
        break;
    }

    // Get the subprogram frame base in case we need it.
    std::optional<Attribute> frame_base =
        subprogram->attribute(DW_AT::DW_AT_frame_base);

    // TODO: Add proper location decoding support.
    if (location->form() != DW_FORM::DW_FORM_exprloc) {
        std::cerr << "Locations in the form of location lists are not "
                     "currently supported.\n";
        return std::nullopt;
    }

    // Decode the frame base for the current variable.
    DwarfLocationStackMachine frame_base_stack_machine;
    DwarfLocation frame_base_value =
        frame_base_stack_machine.exec(frame_base->as_raw());

    // Decode the variable location.
    DwarfLocationStackMachine variable_location_stack_machine;
    DwarfLocation dwarfLocation =
        variable_location_stack_machine.exec(location->as_raw());

    return {dwarfLocation.offset};
}

void Dwarf::read_compile_units() {
    // Get the ELF sections we need and read all of the compile units from
    // the .debug_info section.
    elf::ELFSection debug_info = m_elf->get_section_data(".debug_info");
    elf::ELFSection debug_abbrev = m_elf->get_section_data(".debug_abbrev");
    char* iter = debug_info.data;
    while (std::distance(debug_info.data, iter) < debug_info.size) {
        m_compile_units.emplace_back(&iter, debug_abbrev.data);
    }
}

std::vector<DIE> Dwarf::filter_die_by_tag(DW_TAG tag) {
    std::vector<DIE> filtered;
    for (auto cu : m_compile_units)
        for (DIE die = cu.root(); !die.is_null(); ++die)
            if (die.tag() == tag)
                filtered.emplace_back(die);
    return filtered;
}

std::optional<uint64_t>
Dwarf::debug_line_offset_from_file(std::string_view file) {
    elf::ELFSection debug_str = m_elf->get_section_data(".debug_str");
    std::vector<DIE> entries = filter_die_by_tag(DW_TAG::DW_TAG_compile_unit);
    for (auto entry : entries) {
        if (auto attrib = entry.attribute(DW_AT::DW_AT_name); attrib) {
            auto name = attrib->as_string_view(debug_str.data);
            if (name != file)
                continue;
            auto stmt_list_attrib = entry.attribute(DW_AT::DW_AT_stmt_list);
            if (!stmt_list_attrib)
                return std::nullopt;
            return stmt_list_attrib->as_uint64t();
        }
    }
    return std::nullopt;
}

} // namespace smldbg::dwarf
