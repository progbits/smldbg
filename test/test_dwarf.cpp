#include "gtest/gtest.h"

#include "dwarf.h"

#include <fstream>

namespace {

const char* path = R"(clang-7.0.0/solver/solver)";

using namespace smldbg;

TEST(TestDwarf, ProgramCounter_From_LineAndFile) {
    // Arrange
    auto ifs = std::make_unique<std::ifstream>(path);
    elf::ELF elf(std::move(ifs));
    dwarf::Dwarf dwarf(&elf);

    const std::vector<int> lines = {
        6,  7,  13, 16, 25,        // main.cpp
        10, 13, 16, 21, 29, 30, 33 // solver.cpp
    };

    // Act / Assert
    const std::vector<uint64_t> expected_program_counter_values = {
        0x400ad9, 0x400ad9, 0x400b38, 0x400bb2, 0x400d66, // main.cpp
        0x401756, 0x401778, 0x4017a6, 0x4017fe, 0x4018c2,
        0x4018c2, 0x40194f // solver.cpp
    };

    for (unsigned i = 0, e = lines.size(); i < e; ++i) {
        std::string_view expected_file = i < 5 ? "main.cpp" : "solver.cpp";
        const std::optional<uint64_t> program_counter =
            dwarf.program_counter_from_line_and_file(lines[i], expected_file);
        ASSERT_TRUE(program_counter);
        EXPECT_EQ(*program_counter, expected_program_counter_values[i])
            << "Expected program counter value for line " << std::dec
            << lines[i] << " of " << expected_file << ": " << std::hex
            << expected_program_counter_values[i] << "\n"
            << "Actual program counter value: " << *program_counter;
    }
}

TEST(TestDwarf, SourceLocation_From_Function) {
    // Arrange
    auto ifs = std::make_unique<std::ifstream>(path);
    elf::ELF elf(std::move(ifs));
    dwarf::Dwarf dwarf(&elf);

    const std::vector<std::string> functions = {"main", "knapsack",
                                                "knapsack_impl"};

    // Act / Assert
    const std::vector<uint64_t> expected_addresses = {0x400ad9, 0x4018c2,
                                                      0x401756};

    for (unsigned i = 0, e = functions.size(); i < e; ++i) {
        const std::optional<dwarf::SourceLocation> source_location =
            dwarf.source_location_from_function(functions[i]);
        ASSERT_TRUE(source_location);
        EXPECT_EQ(source_location->address, expected_addresses[i])
            << "Expected source_location for function " << functions[i] << " "
            << std::hex << expected_addresses[i]
            << " Actual source_location: " << source_location->address;
    }
}

TEST(TestDwarf, SourceLocation_From_ProgramCounter) {
    // Arrange
    auto ifs = std::make_unique<std::ifstream>(path);
    elf::ELF elf(std::move(ifs));
    dwarf::Dwarf dwarf(&elf);

    const std::vector<uint64_t> program_counter = {
        0x400ac0, 0x400b3f, 0x400bc5, 0x400c5e, 0x400cec, 0x400d86, // main.cpp
        0x4017e1, 0x401792, 0x4017dd, 0x4017dc, 0x40175c, 0x40189b // solver.cpp
    };

    // Act / Assert
    const std::vector<uint64_t> expected_lines = {
        6,  14, 16, 15, 22, 7, // main.cpp
        20, 15, 18, 20, 12, 27 // solver.cpp
    };

    for (unsigned i = 0, e = program_counter.size(); i < e; ++i) {
        std::string_view expected_file = i < 6 ? "main.cpp" : "solver.cpp";
        const std::optional<dwarf::SourceLocation> source_location =
            dwarf.source_location_from_program_counter(program_counter[i],
                                                       false);
        ASSERT_TRUE(source_location)
            << "Expected source source_location for address " << std::hex
            << program_counter[i] << " is " << expected_file << ":"
            << expected_lines[i];
        EXPECT_EQ(source_location->file, expected_file);
        EXPECT_EQ(source_location->line, expected_lines[i]);
    }

    // Check address outside the range of our program.
    ASSERT_FALSE(dwarf.source_location_from_program_counter(0x400542, false));
}

TEST(TestDwarf, Function_From_ProgramCounter) {
    // Arrange
    auto ifs = std::make_unique<std::ifstream>(path);
    elf::ELF elf(std::move(ifs));
    dwarf::Dwarf dwarf(&elf);

    const std::vector<uint64_t> program_counter_values = {
        0x400ac0, 0x400c1b, 0x400d8b, // main
        0x4018b0, 0x401941, 0x4019cb, // knapsack
        0x401740, 0x401802, 0x4018a4, // knapsack_impl
    };

    // Act / Assert
    const std::vector<std::string_view> expected_functions = {
        "main", "knapsack", "knapsack_impl"};

    for (unsigned i = 0, e = program_counter_values.size(); i < e; ++i) {
        std::string_view expected_function = expected_functions[i / 3];
        const std::optional<std::string> function =
            dwarf.function_from_program_counter(program_counter_values[i]);
        ASSERT_TRUE(function)
            << "Expected a function for program counter = " << std::hex
            << program_counter_values[i];
        EXPECT_EQ(*function, expected_function);
    }
}

} // namespace
