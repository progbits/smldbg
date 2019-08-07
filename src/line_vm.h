#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace smldbg {

struct LineNumberTableRow {
    uint64_t address;
    std::string_view file;
    uint64_t line;
    uint64_t column;
    bool is_stmt;
    bool basic_block;
    bool end_sequence;
    bool prologue_end;
    bool epilogue_begin;
};

class LineVM {

public:
    // Construct a new LineVM instance from the .debug_line section of an ELF
    // file.
    //
    // Preconditions: |debug_line| should point to the first byte of the line
    // number header for the compile unit. |debug_str| should point to the first
    // byte of the .debug_str ELF section.
    //
    // Postconditions: None.
    LineVM(char* debug_line, char* debug_str);

    // Run the virtual machine and generate the line number table.
    void exec();

    // Get the line number table
    std::vector<LineNumberTableRow> table();

private:
    // Line number program header.
    // Section 6.2.4
    // http://www.dwarfstd.org/doc/DWARF4.pdf
    struct Header {
        Header()
            : is_64bit(false), unit_length(0), version(0), header_length(0),
              minimum_instruction_length(0),
              maximum_operations_per_instruction(0), default_is_stmt(false),
              line_base(0), line_range(0), opcode_base(0),
              standard_opcode_lengths(0) {}

        bool is_64bit;
        uint64_t unit_length;
        uint16_t version;
        uint64_t header_length;
        uint8_t minimum_instruction_length;
        uint8_t maximum_operations_per_instruction;
        uint8_t default_is_stmt;
        int8_t line_base;
        uint8_t line_range;
        uint8_t opcode_base;
        std::vector<uint8_t> standard_opcode_lengths;
        std::vector<std::string_view> include_paths;
        std::vector<std::string_view> file_names;
    };

    // Opcodes.
    // Section 6.2.5.2
    // http://www.dwarfstd.org/doc/DWARF4.pdf
    enum OpCode {
        DW_LNS_copy = 0x01,
        DW_LNS_advance_pc = 0x02,
        DW_LNS_advance_line = 0x03,
        DW_LNS_set_file = 0x04,
        DW_LNS_set_column = 0x05,
        DW_LNS_negate_stmt = 0x06,
        DW_LNS_set_basic_block = 0x07,
        DW_LNS_const_add_pc = 0x08,
        DW_LNS_fixed_advance_pc = 0x09,
        DW_LNS_set_prologue_end = 0x0A,
        DW_LNS_set_epilogue_begin = 0x0B,
        DW_LNS_set_isa = 0x0C,
    };

    // Extended opcodes.
    // Section 6.2.5.3
    // http://www.dwarfstd.org/doc/DWARF4.pdf
    enum ExtendedOpCode {
        DW_LNE_end_sequence = 0x01,
        DW_LNE_set_address = 0x02,
        DW_LNE_define_file = 0x03,
        DW_LNE_set_discriminator = 0x04,
        DW_LNE_lo_user = 0x80,
        DW_LNE_hi_user = 0xFF,
    };

    // State machine registers.
    // Section 6.2.1
    // http://www.dwarfstd.org/doc/DWARF4.pdf
    struct Registers {
        Registers(bool is_stmt = false)
            : address(0), op_index(0), file(1), line(1), column(0),
              is_stmt(is_stmt), basic_block(false), end_sequence(false),
              prologue_end(false), epilogue_begin(false), isa(0),
              discriminator(0) {}

        // Reset registers to their default values. The default value for
        // |is_stmt| is determined by the line number program header.
        void reset() {
            address = 0;
            op_index = 0;
            file = 1;
            line = 1;
            column = 0;
            // is_stmt(is_stmt);
            basic_block = false;
            end_sequence = false;
            prologue_end = false;
            epilogue_begin = false;
            isa = 0;
            discriminator = 0;
        }

        uint64_t address;
        uint64_t op_index;
        uint64_t file;
        uint64_t line;
        uint64_t column;
        bool is_stmt;
        bool basic_block;
        bool end_sequence;
        bool prologue_end;
        bool epilogue_begin;
        uint64_t isa;
        uint64_t discriminator;
    };

    // Read the line number program header. After the line number program header
    // has been read, |m_instructions| points to the first byte of the first
    // opcode.
    void read_header();

    Header m_header; // The header for the .debug_line entry.

    char* m_debug_line; // Points to the first byte of the .debug_line entry.

    char* m_debug_line_end; // Set by |read_header()|. Points to the last byte
                            // of the .debug_line entry. Use to check if we have
                            // consume all of this entries opcodes.

    char* m_instructions; // Set by |read_header()|. Points to the first byte of
                          // the first opcode.

    char* m_debug_str; // The first byte of the .debug_str ELF section.

    std::vector<Registers> m_state; // Commited registers.
};

} // namespace smldbg
