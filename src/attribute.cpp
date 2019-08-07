#include "attribute.h"

namespace smldbg::dwarf {

Attribute::Attribute(DW_FORM form, char* debug_info)
    : m_form(form), m_debug_info(debug_info) {}

void Attribute::eat(DW_FORM form, char*& data, bool is_64bit) {
    switch (form) {
    // TODO: Use platform address size.
    case DW_FORM::DW_FORM_addr:
        std::advance(data, sizeof(uint64_t));
        break;
    case DW_FORM::DW_FORM_data2:
    case DW_FORM::DW_FORM_ref2:
        std::advance(data, sizeof(uint16_t));
        break;
    case DW_FORM::DW_FORM_data4:
    case DW_FORM::DW_FORM_ref4:
        std::advance(data, sizeof(uint32_t));
        break;
    case DW_FORM::DW_FORM_data8:
    case DW_FORM::DW_FORM_ref8:
        std::advance(data, sizeof(uint64_t));
        break;
    case DW_FORM::DW_FORM_data1:
    case DW_FORM::DW_FORM_flag:
    case DW_FORM::DW_FORM_ref1:
        std::advance(data, sizeof(uint8_t));
        break;
    case DW_FORM::DW_FORM_strp:
    case DW_FORM::DW_FORM_ref_addr:
    case DW_FORM::DW_FORM_sec_offset: {
        const uint64_t size = is_64bit ? sizeof(uint64_t) : sizeof(uint32_t);
        std::advance(data, size);
        break;
    }
    case DW_FORM::DW_FORM_sdata:
        util::decodeLEB128(data);
        break;
    case DW_FORM::DW_FORM_udata:
        util::decodeULEB128(data);
        break;
    case DW_FORM::DW_FORM_block:
    case DW_FORM::DW_FORM_exprloc: {
        const uint64_t size = util::decodeULEB128(data);
        std::advance(data, size);
        break;
    }
    case DW_FORM::DW_FORM_flag_present:
        break; // No associated data.
    case DW_FORM::DW_FORM_block1: {
        uint8_t length = 0;
        std::memcpy(&length, data, sizeof(length));
        std::advance(data, length + sizeof(length));
        break;
    }
    case DW_FORM::DW_FORM_block2: {
        uint16_t length = 0;
        std::memcpy(&length, data, sizeof(length));
        std::advance(data, length + sizeof(length));
        break;
    }
    case DW_FORM::DW_FORM_block4: {
        uint32_t length = 0;
        std::memcpy(&length, data, sizeof(length));
        std::advance(data, length + sizeof(length));
        break;
    }
    case DW_FORM::DW_FORM_ref_udata:
    case DW_FORM::DW_FORM_indirect:
    case DW_FORM::DW_FORM_string:
    case DW_FORM::DW_FORM_ref_sig8:
    case DW_FORM::DW_FORM_null:
        std::cerr << "Unsupported DW_FORM type.\n";
        std::exit(1);
    }
}

uint64_t Attribute::as_uint64t() {
    uint64_t value = 0;
    switch (m_form) {
    case DW_FORM::DW_FORM_addr:
    case DW_FORM::DW_FORM_data4:
        std::memcpy(&value, m_debug_info, sizeof(uint32_t));
        break;
    case DW_FORM::DW_FORM_sec_offset:
        std::memcpy(&value, m_debug_info, sizeof(uint32_t));
        break;
    default:
        std::cerr << "Unsupported DW_FORM type.\n";
        std::exit(1);
    }
    return value;
}

std::string_view Attribute::as_string_view(char* debug_str) {
    switch (m_form) {
    case DW_FORM::DW_FORM_string:
        // Data is in the form of a null terminated string.
        return std::string_view(m_debug_info);
    case DW_FORM::DW_FORM_strp: {
        // Data is in the form of an offset into the .debug_str section.
        uint32_t offset = 0;
        std::memcpy(&offset, m_debug_info, sizeof(offset));
        return std::string_view(debug_str + offset);
    }
    default:
        std::cerr << "Trying to stringify an unsupported DW_FORM.\n";
        std::exit(1);
    }
}

std::vector<char> Attribute::as_raw() {
    switch (m_form) {
    case DW_FORM::DW_FORM_exprloc: {
        uint64_t size = util::decodeULEB128(m_debug_info);
        std::vector<char> bytes(size);
        std::memcpy(bytes.data(), m_debug_info, size);
        return bytes;
    }
    default:
        std::cerr
            << "Trying to extract raw bytes from an unsupported DW_FORM.\n";
        std::exit(1);
    }
}

} // namespace smldbg::dwarf