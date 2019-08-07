#pragma once

#include "attribute.h"
#include "util.h"

#include <algorithm>
#include <optional>
#include <vector>

namespace smldbg::dwarf {

// Section 7
// http://www.dwarfstd.org/doc/DWARF4.pdf
enum class DW_TAG {
    DW_TAG_null = 0x0,
    DW_TAG_array_type = 0x01,
    DW_TAG_class_type = 0x02,
    DW_TAG_entry_point = 0x03,
    DW_TAG_enumeration_type = 0x04,
    DW_TAG_formal_parameter = 0x05,
    DW_TAG_imported_declaration = 0x08,
    DW_TAG_label = 0x0a,
    DW_TAG_lexical_block = 0x0b,
    DW_TAG_member = 0x0d,
    DW_TAG_pointer_type = 0x0f,
    DW_TAG_reference_type = 0x10,
    DW_TAG_compile_unit = 0x11,
    DW_TAG_string_type = 0x12,
    DW_TAG_structure_type = 0x13,
    DW_TAG_subroutine_type = 0x15,
    DW_TAG_typedef = 0x16,
    DW_TAG_union_type = 0x17,
    DW_TAG_unspecified_parameters = 0x18,
    DW_TAG_variant = 0x19,
    DW_TAG_common_block = 0x1a,
    DW_TAG_common_inclusion = 0x1b,
    DW_TAG_inheritance = 0x1c,
    DW_TAG_inlined_subroutine = 0x1d,
    DW_TAG_module = 0x1e,
    DW_TAG_ptr_to_member_type = 0x1f,
    DW_TAG_set_type = 0x20,
    DW_TAG_subrange_type = 0x21,
    DW_TAG_with_stmt = 0x22,
    DW_TAG_access_declaration = 0x23,
    DW_TAG_base_type = 0x24,
    DW_TAG_catch_block = 0x25,
    DW_TAG_const_type = 0x26,
    DW_TAG_constant = 0x27,
    DW_TAG_enumerator = 0x28,
    DW_TAG_file_type = 0x29,
    DW_TAG_friend = 0x2a,
    DW_TAG_namelist = 0x2b,
    DW_TAG_namelist_item = 0x2c,
    DW_TAG_packed_type = 0x2d,
    DW_TAG_subprogram = 0x2e,
    DW_TAG_template_type_parameter = 0x2f,
    DW_TAG_template_value_parameter = 0x30,
    DW_TAG_thrown_type = 0x31,
    DW_TAG_try_block = 0x32,
    DW_TAG_variant_part = 0x33,
    DW_TAG_variable = 0x34,
    DW_TAG_volatile_type = 0x35,
    DW_TAG_dwarf_procedure = 0x36,
    DW_TAG_restrict_type = 0x37,
    DW_TAG_interface_type = 0x38,
    DW_TAG_namespace = 0x39,
    DW_TAG_imported_module = 0x3a,
    DW_TAG_unspecified_type = 0x3b,
    DW_TAG_partial_unit = 0x3c,
    DW_TAG_imported_unit = 0x3d,
    DW_TAG_condition = 0x3f,
    DW_TAG_shared_type = 0x40,
    DW_TAG_type_unit = 0x41,
    DW_TAG_rvalue_reference_type = 0x42,
    DW_TAG_template_alias = 0x43,
    DW_TAG_lo_user = 0x4080,
    DW_TAG_hi_user = 0xffff,
};

// Section 7
// http://www.dwarfstd.org/doc/DWARF4.pdf
enum class DW_CHLIDREN {
    DW_CHILDREN_no = 0x00,
    DW_CHILDREN_yes = 0x01,
};

class DIE {

public:
    // Construct a new DIE from a compile units .debug_info entry.
    //
    // Preconditions: |debug_info| should point to the first byte of the DW_TAG
    // for the entry. It is normally most useful to construct a new DIE instance
    // with the first entry of a compile unit and use operator++() to
    // iterate the compile units tags.
    //
    // Postconditions : None.
    DIE(char* debug_info, char* debug_info_end, char* debug_abbrev,
        bool is_64bit);

    // Return the tag associated with this entry.
    DW_TAG tag() { return m_ate.tag; }

    // Return the attribute if present.
    std::optional<Attribute> attribute(DW_AT attribute);

    // Our DIE is null when we have reached the end of the .debug_info section
    // for the associated compile unit.
    bool is_null() { return m_debug_info == m_debug_info_end; }

    std::vector<DIE> get_nested();

    // Step to the next entry. Callers should check is_null() after each
    // increment.
    // TODO: Abstract a DIE iterator to make this a bit more useful.
    DIE& operator++();

private:
    struct AbbreviationTableEntry {
        DW_TAG tag;
        DW_CHLIDREN has_children;
        std::vector<DW_AT> attributes;
        std::vector<DW_FORM> forms;
    };

    // Eat the data from the abbreviations associated with the current entry.
    //
    // Preconditions: |m_debug_info| should point to the first byte of the data
    // associated with the current entry. i.e. after read_abbreviation_code().
    //
    // Postconditions: |m_debug_info| is advanced past the data for the current
    // entry.
    void eat_entry();

    // Read the abbreviation code for the current entry.
    //
    // Preconditions: |m_debug_info| should point to the first byte of the
    // abbreviation code for the current entry.
    //
    // Postconditions: |m_debug_info| is advanced to the first byte of the data
    // associated with the current entry.
    void read_abbreviation_code();

    // Decode the tag, attributes and forms of the abbreviation |index| from the
    // .debug_abbrev section of the parent compile unit. If the abbreviation
    // table for the associated compile unit doesn't contain an entry with
    // |index|, an entry with DW_TAG_null is returned.
    //
    // Preconditions: |m_debug_abbrev| should point to the first byte of the
    // .debug_abbrev section of the associated compile unit.
    //
    // Postconditons: None.
    AbbreviationTableEntry decode_abbreviations(uint64_t index);

    std::vector<DW_AT>::iterator find_attribute(DW_AT attribute);

    char* m_debug_info; // Points to the first byte of the .debug_info entry for
                        // the associated compile unit.

    char* m_debug_info_end; // Points to the last byte of the .debug_info entry
                            // for the associated compile unit.

    char* m_debug_abbrev; // Points to the first byte of the .debug_abbrev entry
                          // for the associated compile unit.

    bool m_is_64bit; // Status of the is_64bit flag of the compile unit
                     // associated with this entry.

    AbbreviationTableEntry m_ate; // Contents of the .debug_abbrev section for
                                  // the index associated with this DIE.
};

} // namespace smldbg::dwarf
