cmake_minimum_required (VERSION 3.12.4)

# Dependencies.
set(GOOGLE_TEST_INCLUDE_DIR "" CACHE PATH "")
set(GOOGLE_TEST_LIBRARY_DIR "" CACHE PATH "")
set(GOOGLE_MOCK_INCLUDE_DIR "" CACHE PATH "")
set(GOOGLE_MOCK_LIBRARY_DIR "" CACHE PATH "")

add_executable(test_smldbg
    ${CMAKE_SOURCE_DIR}/src/elf.cpp
    ${CMAKE_SOURCE_DIR}/src/dwarf.cpp
    ${CMAKE_SOURCE_DIR}/src/util.cpp
    test_driver.cpp
    test_dwarf.cpp
    test_util.cpp)

target_include_directories(test_smldbg PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${GOOGLETEST_INCLUDE_DIR}
    ${GOOGLEMOCK_INCLUDE_DIR})

target_link_libraries(test_smldbg gtest gmock smldbg)

add_test(NAME test_smldbg COMMAND test_smldbg)
