#include "line_vm.h"

#include "util.h"

#include <cassert>
#include <iostream>

namespace smldbg {

LineVM::LineVM(char* debug_line, char* debug_str)
    : m_debug_line(debug_line), m_debug_line_end(debug_line),
      m_debug_str(debug_str) {
    read_header();
}

void LineVM::exec() {
    char* iter = m_instructions;
    Registers registers(m_header.default_is_stmt);
    while (true) {
        // Fetch the current opcode.
        const uint8_t opcode = util::read_bytes<uint8_t>(iter);

        // Handle extended opcodes.
        if (opcode == 0x00) {
            const auto length = util::decodeULEB128(iter);
            const auto extended_opcode = util::read_bytes<uint8_t>(iter);
            switch (extended_opcode) {
            case (DW_LNE_end_sequence):
                registers.end_sequence = true;
                m_state.push_back(registers);
                registers.reset();
                if (iter == m_debug_line_end)
                    return;
                break;
            case (DW_LNE_set_address): {
                registers.address = util::read_bytes<uint64_t>(iter);
                registers.op_index = 0;
                break;
            }
            case (DW_LNE_define_file):
            case (DW_LNE_set_discriminator):
            case (DW_LNE_lo_user):
            case (DW_LNE_hi_user):
                // TODO: Add support for these opcodes.
                std::cerr << "Unimplemented extended opcode.\n";
                std::exit(1);
            }

            // Advance to the next opcode.
            continue;
        }

        // Handle special opcodes.
        if (opcode > m_header.opcode_base) {
            const uint8_t adjusted = opcode - m_header.opcode_base;

            const uint64_t address_increment =
                (adjusted / m_header.line_range) *
                m_header.minimum_instruction_length;

            const uint64_t line_increment =
                m_header.line_base + (adjusted % m_header.line_range);

            registers.address += address_increment;
            registers.line += line_increment;

            // Commit the current state.
            m_state.push_back(registers);

            registers.basic_block = false;
            registers.prologue_end = false;
            registers.epilogue_begin = false;
            registers.discriminator = 0;

            // Advance to the next opcode.
            continue;
        }

        // Handle standard opcodes.
        switch (opcode) {
        case (DW_LNS_copy): {
            m_state.push_back(registers);
            registers.discriminator = 0;
            registers.basic_block = false;
            registers.prologue_end = false;
            registers.epilogue_begin = false;
            break;
        }
        case (DW_LNS_advance_pc): {
            auto operationAdvance = util::decodeULEB128(iter);

            registers.address = registers.address +
                                m_header.minimum_instruction_length *
                                    (registers.op_index + operationAdvance) /
                                    m_header.maximum_operations_per_instruction;

            registers.op_index = (registers.op_index + operationAdvance) %
                                 m_header.maximum_operations_per_instruction;

            registers.basic_block = false;
            registers.prologue_end = false;
            registers.epilogue_begin = false;
            registers.discriminator = 0;
            break;
        }
        case (DW_LNS_advance_line): {
            registers.line += util::decodeLEB128(iter);
            break;
        }
        case (DW_LNS_set_file): {
            registers.file = util::decodeULEB128(iter);
            break;
        }
        case (DW_LNS_set_column): {
            registers.column = util::decodeULEB128(iter);
            break;
        }
        case (DW_LNS_negate_stmt): {
            registers.is_stmt = !registers.is_stmt;
            break;
        }
        case (DW_LNS_set_basic_block): {
            registers.basic_block = true;
            break;
        }
        case (DW_LNS_const_add_pc): {
            const uint8_t adjusted = 255 - m_header.opcode_base;
            const uint64_t address_increment =
                (adjusted / m_header.line_range) *
                m_header.minimum_instruction_length;
            registers.address += address_increment;
            break;
        }
        case (DW_LNS_fixed_advance_pc): {
            registers.address += util::read_bytes<uint16_t>(iter);
            registers.op_index = 0;
            break;
        }
        case (DW_LNS_set_prologue_end): {
            registers.prologue_end = true;
            break;
        }
        case (DW_LNS_set_epilogue_begin): {
            registers.epilogue_begin = true;
            break;
        }
        case (DW_LNS_set_isa): {
            registers.isa = util::decodeULEB128(iter);
            break;
        }
        }
    }
}

std::vector<LineNumberTableRow> LineVM::table() {
    std::vector<LineNumberTableRow> rows(m_state.size());
    for (unsigned i = 0; const auto& row : m_state) {
        rows[i++] = {
            .address = row.address,
            .file = m_header.file_names[row.file - 1], // 1 indexed.
            .line = row.line,
            .column = row.column,
            .is_stmt = row.is_stmt,
            .basic_block = row.basic_block,
            .end_sequence = row.end_sequence,
            .prologue_end = row.prologue_end,
            .epilogue_begin = row.epilogue_begin,
        };
    }
    return rows;
}

void LineVM::read_header() {
    char* iter = m_debug_line;
    uint32_t maybe_padding = util::read_bytes<uint32_t>(iter);
    if (maybe_padding == 0xFFFFFFFF) {
        m_header.is_64bit = true;
        m_header.unit_length = util::read_bytes<uint64_t>(iter);
    } else {
        m_header.is_64bit = false;
        m_header.unit_length = maybe_padding;
    }
    m_debug_line_end =
        m_debug_line + m_header.unit_length + (m_header.is_64bit ? 12 : 4);

    // Dwarf version.
    m_header.version = util::read_bytes<uint16_t>(iter);
    if (m_header.is_64bit)
        m_header.header_length = util::read_bytes<uint64_t>(iter);
    else
        m_header.header_length = util::read_bytes<uint32_t>(iter);

    // Use |header.header_length| to determine the header end pointer.
    auto header_end_iter = iter;
    std::advance(header_end_iter, m_header.header_length);

    m_header.minimum_instruction_length = util::read_bytes<uint8_t>(iter);

    // This is only include post Dwarf V4.
    if (m_header.version >= 4)
        m_header.maximum_operations_per_instruction =
            util::read_bytes<uint8_t>(iter);

    m_header.default_is_stmt = util::read_bytes<uint8_t>(iter);
    m_header.line_base = util::read_bytes<int8_t>(iter);
    m_header.line_range = util::read_bytes<uint8_t>(iter);
    m_header.opcode_base = util::read_bytes<uint8_t>(iter);

    // We don't care about opcode lengths.
    std::advance(iter, sizeof(uint8_t) * m_header.opcode_base - 1);

    // Decode the include paths. Sequence terminates with "\0\0".
    while (iter != header_end_iter) {
        if (std::string_view path(iter); path.length() > 0) {
            std::advance(iter, path.length() + 1);
            m_header.include_paths.emplace_back(path);
        } else
            break;
    }
    std::advance(iter, sizeof(uint8_t)); // Skip the terminating '\0'

    // Decode the file names.
    while (iter != header_end_iter) {
        if (std::string_view file_name(iter); file_name.length() > 0) {
            std::advance(iter, file_name.length() + 1);
            m_header.file_names.emplace_back(file_name);

            // Munch other parameters.
            util::decodeULEB128(iter);
            util::decodeULEB128(iter);
            util::decodeULEB128(iter);
        } else
            break;
    }
    std::advance(iter, sizeof(uint8_t)); // Skip the terminating '\0'.

    // If we've not consumed the whole header, something has gone wrong.
    assert(iter == header_end_iter);
    m_instructions = iter;
}

} // namespace smldbg
