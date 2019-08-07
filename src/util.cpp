#include "util.h"

#include <cstring>
#include <string>
#include <vector>

namespace smldbg::util {

// Decode unsigned Little Endian Base 128 encoded integer.
// http://www.dwarfstd.org/doc/DWARF4.pdf
uint64_t decodeULEB128(char*& iter) {
    uint64_t result = 0;
    uint8_t shift = 0;
    while (true) {
        uint8_t byte = *iter++;
        result |= ((byte & 0b01111111) << shift);
        if ((byte >> 7) == 0)
            break;
        shift += 7;
    }
    return result;
}

// Decode signed Little Endian Base 128 encoded integer.
// http://www.dwarfstd.org/doc/DWARF4.pdf
int64_t decodeLEB128(char*& iter) {
    int64_t result = 0;
    uint8_t shift = 0;
    const uint8_t size = 64;
    uint8_t byte = *iter++;
    while (true) {
        result |= ((byte & 0b01111111) << shift);
        shift += 7;
        if ((byte & 0b10000000) == 0)
            break;
        byte = *iter++;
    }

    // sign bit is second high order bit (0x40)
    if ((shift < size) && (byte & 0x40))
        result |= (~0ULL << shift);

    return result;
}

// Split |input| by delimiter and return the resulting collection of tokens.
std::vector<std::string> tokenize(const std::string& input, char delimiter) {
    std::vector<std::string> tokens;
    decltype(input.size()) last = 0;
    decltype(input.size()) next = 0;
    while ((next = input.find(delimiter, last)) != std::string::npos) {
        tokens.emplace_back(input.begin() + last, input.begin() + next);
        last = ++next;
    }
    tokens.emplace_back(input.begin() + last, input.end());
    return tokens;
}

} // namespace smldbg::util
