#include "die.h"

namespace smldbg::dwarf {

DIE::DIE(char* debug_info, char* debug_info_end, char* debug_abbrev,
         bool is_64bit)
    : m_debug_info(debug_info), m_debug_info_end(debug_info_end),
      m_debug_abbrev(debug_abbrev), m_is_64bit(is_64bit) {
    read_abbreviation_code();
}

std::optional<Attribute> DIE::attribute(DW_AT attribute) {
    // Eat bytes until |data| points to the first byte of the form for the
    // requested attribute.
    const auto entry = find_attribute(attribute);
    if (entry == m_ate.attributes.end())
        return std::nullopt;
    const auto index = std::distance(m_ate.attributes.begin(), entry);
    char* data = m_debug_info;
    for (unsigned i = 0; i < index; ++i) {
        Attribute::eat(m_ate.forms[i], data, m_is_64bit);
    }
    return Attribute(m_ate.forms[index], data);
}

std::vector<DIE> DIE::get_nested() {
    // Get the next debug information entry.
    DIE die = *this;
    ++die;

    // If the next entry is null or the current entry has no children, there are
    // no nested entries to extract.
    if (die.is_null() || m_ate.has_children == DW_CHLIDREN::DW_CHILDREN_no)
        return {};

    // Extract nested entries until we return to the current nesting depth.
    int depth = 0;
    std::vector<DIE> nested;
    do {
        if (die.m_ate.has_children == DW_CHLIDREN::DW_CHILDREN_yes)
            ++depth;
        else if (die.tag() == DW_TAG::DW_TAG_null)
            --depth;

        if (die.tag() != DW_TAG::DW_TAG_null)
            nested.push_back(die);
        ++die;
    } while (!die.is_null() && depth > 0);

    return nested;
}

DIE& DIE::operator++() {
    // Eat the current entry.
    eat_entry();
    // Read the code for the new entry we now represent.
    read_abbreviation_code();
    return *this;
}

void DIE::eat_entry() {
    for (unsigned i = 0, e = m_ate.attributes.size(); i < e; ++i) {
        Attribute::eat(m_ate.forms[i], m_debug_info, m_is_64bit);
    }
}

void DIE::read_abbreviation_code() {
    // Read the tag for the entry. After we've read the tag, |m_debug_info|
    // points to the first byte of the first attribute. Use the
    // AbbreviationTableEntry instance returned by decode_abbreviations(...)
    // to interpret the data.
    const uint64_t tag_index = util::decodeULEB128(m_debug_info);
    if (tag_index > 0)
        m_ate = decode_abbreviations(tag_index);
    else
        m_ate = {}; // Null entry.
}

DIE::AbbreviationTableEntry DIE::decode_abbreviations(uint64_t index) {
    char* iter = m_debug_abbrev;
    while (true) {
        // Decode the index of the current tag.
        const uint64_t entry_index = util::decodeULEB128(iter);

        // Decode the tag. A null tag indicates we have reached the end of
        // the abbreviation table for the associated compile unit.
        AbbreviationTableEntry ate = {};
        ate.tag = static_cast<DW_TAG>(util::decodeULEB128(iter));
        if (ate.tag == DW_TAG::DW_TAG_null)
            return ate;

        // Does this tag have any children or is the next entry a sibling?
        ate.has_children =
            static_cast<DW_CHLIDREN>(util::read_bytes<char>(iter));

        // Decode the tags attributes and their forms. A null entry for both
        // the attribute and form indicate we have reached the end of the
        // current table entry.
        DW_AT att = static_cast<DW_AT>(util::decodeULEB128(iter));
        DW_FORM form = static_cast<DW_FORM>(util::decodeULEB128(iter));
        while (att != DW_AT::DW_AT_null && form != DW_FORM::DW_FORM_null) {
            ate.attributes.push_back(att);
            ate.forms.push_back(form);
            att = static_cast<DW_AT>(util::decodeULEB128(iter));
            form = static_cast<DW_FORM>(util::decodeULEB128(iter));
        }

        if (entry_index == index)
            return ate;
    }
}

std::vector<DW_AT>::iterator DIE::find_attribute(DW_AT attribute) {
    return std::find_if(m_ate.attributes.begin(), m_ate.attributes.end(),
                        [&](DW_AT entry) { return entry == attribute; });
}

} // namespace smldbg::dwarf