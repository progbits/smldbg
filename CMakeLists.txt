cmake_minimum_required (VERSION 3.12.4)
project ("smldbg")

set(CMAKE_CXX_STANDARD 20)

enable_testing()
add_subdirectory(test)

add_library (smldbg
    ${CMAKE_SOURCE_DIR}/src/elf.cpp
    ${CMAKE_SOURCE_DIR}/src/dwarf.cpp
    ${CMAKE_SOURCE_DIR}/src/compile_unit.cpp
    ${CMAKE_SOURCE_DIR}/src/attribute.cpp
    ${CMAKE_SOURCE_DIR}/src/die.cpp
    ${CMAKE_SOURCE_DIR}/src/line_vm.cpp
    ${CMAKE_SOURCE_DIR}/src/dwarf_location_stack_machine.cpp
    ${CMAKE_SOURCE_DIR}/src/command_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/breakpoint.cpp
    ${CMAKE_SOURCE_DIR}/src/debugger.cpp
    ${CMAKE_SOURCE_DIR}/src/util.cpp)

add_executable (driver ${CMAKE_SOURCE_DIR}/src/main.cpp)
target_link_libraries(driver smldbg)
