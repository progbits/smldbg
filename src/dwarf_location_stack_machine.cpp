#include "dwarf_location_stack_machine.h"

#include "util.h"

#include <iostream>

namespace smldbg::dwarf {

DwarfLocation DwarfLocationStackMachine::exec(std::vector<char> instructions) {
    // Very, very basic exprloc decoding. Currently only supports frame base
    // relative values.
    // TODO: Decoding of more complex expressions.
    char* iter = instructions.data();
    char* end = instructions.data() + instructions.size();

    // Decode the first opcode of the instruction stream.
    const Opcode opcode = static_cast<Opcode>(util::read_bytes<uint8_t>(iter));

    // Handle reg based opcodes.
    if (is_reg_opcode(opcode))
        return handle_reg_opcode(opcode);

    // Handle breg based opcodes.
    if (is_breg_opcode(opcode))
        return handle_breg_opcode(opcode, iter);

    // Handle all other opcodes.
    while (iter != end) {
        switch (opcode) {
        case Opcode::DW_OP_addr:
        case Opcode::DW_OP_deref:
        case Opcode::DW_OP_const1u:
        case Opcode::DW_OP_const1s:
        case Opcode::DW_OP_const2u:
        case Opcode::DW_OP_const2s:
        case Opcode::DW_OP_const4u:
        case Opcode::DW_OP_const4s:
        case Opcode::DW_OP_const8u:
        case Opcode::DW_OP_const8s:
        case Opcode::DW_OP_constu:
        case Opcode::DW_OP_consts:
        case Opcode::DW_OP_dup:
        case Opcode::DW_OP_drop:
        case Opcode::DW_OP_over:
        case Opcode::DW_OP_pick:
        case Opcode::DW_OP_swap:
        case Opcode::DW_OP_rot:
        case Opcode::DW_OP_xderef:
        case Opcode::DW_OP_abs:
        case Opcode::DW_OP_and:
        case Opcode::DW_OP_div:
        case Opcode::DW_OP_minus:
        case Opcode::DW_OP_mod:
        case Opcode::DW_OP_mul:
        case Opcode::DW_OP_neg:
        case Opcode::DW_OP_not:
        case Opcode::DW_OP_or:
        case Opcode::DW_OP_plus:
        case Opcode::DW_OP_plus_uconst:
        case Opcode::DW_OP_shl:
        case Opcode::DW_OP_shr:
        case Opcode::DW_OP_shra:
        case Opcode::DW_OP_xor:
        case Opcode::DW_OP_skip:
        case Opcode::DW_OP_bra:
        case Opcode::DW_OP_eq:
        case Opcode::DW_OP_ge:
        case Opcode::DW_OP_gt:
        case Opcode::DW_OP_le:
        case Opcode::DW_OP_lt:
        case Opcode::DW_OP_ne:
        case Opcode::DW_OP_lit0:
        case Opcode::DW_OP_lit1: // Handle values 2...30
        case Opcode::DW_OP_lit31:
        case Opcode::DW_OP_regx:
            std::cerr << "Trying to decode unsupported DW_OP_regx.\n";
            std::exit(1);
        case Opcode::DW_OP_fbreg: {
            const int64_t offset = util::decodeLEB128(iter);
            return DwarfLocation{.base = DwarfLocationBase::FrameBase,
                                 .offset = offset};
        }
        case Opcode::DW_OP_bregx:
        case Opcode::DW_OP_piece:
        case Opcode::DW_OP_deref_size:
        case Opcode::DW_OP_xderef_size:
        case Opcode::DW_OP_nop:
        case Opcode::DW_OP_push_object_address:
        case Opcode::DW_OP_call2:
        case Opcode::DW_OP_call4:
        case Opcode::DW_OP_call_ref:
        case Opcode::DW_OP_form_tls_address:
        case Opcode::DW_OP_call_frame_cfa:
        case Opcode::DW_OP_bit_piece:
        case Opcode::DW_OP_implicit_value:
        case Opcode::DW_OP_stack_value:
        case Opcode::DW_OP_lo_user:
        case Opcode::DW_OP_hi_user:
            std::cerr << "Trying to decode unsupported DW_OP.\n";
            std::exit(1);
        }
    }
    return {};
}

typename std::underlying_type<DwarfLocationStackMachine::Opcode>::type
DwarfLocationStackMachine::op_to_int(Opcode opcode) {
    return static_cast<typename std::underlying_type<Opcode>::type>(opcode);
}

bool DwarfLocationStackMachine::is_reg_opcode(Opcode opcode) {
    return (op_to_int(opcode) >= op_to_int(Opcode::DW_OP_reg0) &&
            op_to_int(opcode) <= op_to_int(Opcode::DW_OP_reg31));
}

bool DwarfLocationStackMachine::is_breg_opcode(Opcode opcode) {
    return (op_to_int(opcode) >= op_to_int(Opcode::DW_OP_breg0) &&
            op_to_int(opcode) <= op_to_int(Opcode::DW_OP_breg31));
}

DwarfLocation DwarfLocationStackMachine::handle_reg_opcode(Opcode opcode) {
    return DwarfLocation{
        .base = DwarfLocationBase::Register,
        .register_index = op_to_int(opcode) - op_to_int(Opcode::DW_OP_reg0),
        .offset = std::numeric_limits<decltype(DwarfLocation::offset)>::max(),
        .address = std::numeric_limits<decltype(DwarfLocation::offset)>::max(),
    };
}

DwarfLocation DwarfLocationStackMachine::handle_breg_opcode(Opcode opcode,
                                                            char* iter) {
    // DW_OP_breg0 instructions have a single SLEB128 operand.
    const int64_t offset = util::decodeLEB128(iter);
    return DwarfLocation{
        .base = DwarfLocationBase::Register,
        .register_index = op_to_int(opcode) - op_to_int(Opcode::DW_OP_breg0),
        .offset = offset,
        .address = std::numeric_limits<decltype(DwarfLocation::offset)>::max(),
    };
}

} // namespace smldbg::dwarf
