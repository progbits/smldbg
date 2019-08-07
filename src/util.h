#pragma once

#include <cstring>
#include <string>
#include <vector>

namespace smldbg::util {

template <typename T> T read_bytes(char*& bytes) {
    T value = {};
    std::memcpy(&value, bytes, sizeof(T));
    std::advance(bytes, sizeof(T));
    return value;
}

template <typename T> T read_bytes(std::vector<char>::iterator& iter) {
    T value = {};
    std::memcpy(&value, &(*iter), sizeof(T));
    std::advance(iter, sizeof(T));
    return value;
}

// Decode unsigned Little Endian Base 128 encoded integer.
// http://www.dwarfstd.org/doc/DWARF4.pdf
uint64_t decodeULEB128(char*& iter);

// Decode signed Little Endian Base 128 encoded integer.
// http://www.dwarfstd.org/doc/DWARF4.pdf
int64_t decodeLEB128(char*& iter);

// Split |input| by delimiter and return the resulting collection of tokens.
std::vector<std::string> tokenize(const std::string& input, char delimiter);

} // namespace smldbg::util
