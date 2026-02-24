cmake_minimum_required(VERSION 3.20)

project(kvdb
    VERSION             0.5.0
    DESCRIPTION         "File-backed key-value store with sequential log"
    LANGUAGES           CXX
)

enable_testing()

# --- Standard ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)     # error if compiler can't do C++20
set(CMAKE_CXX_EXTENSIONS OFF)           # use -std=c++20, not -std=gnu++20

# --- Version header ---
configure_file(
    ${CMAKE_SOURCE_DIR}/include/version.h.in
    ${CMAKE_BINARY_DIR}/include/version.h
)

# --- Library sources ---
set(LIB_SOURCES
    src/entry.cpp
    src/log.cpp
    src/kv.cpp
)

# Platform-specific source
if(UNIX)
    list(APPEND LIB_SOURCES src/platform_unix.cpp)
elseif(WIN32)
    list(APPEND LIB_SOURCES src/platform_windows.cpp)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# --- Build the library ---
# STATIC: compiled once, linked directly into the test binary
add_library(kvdb_lib STATIC ${LIB_SOURCES})

target_include_directories(kvdb_lib
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_BINARY_DIR}/include
)

target_compile_options(kvdb_lib PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# --- Test executable ---
find_package(GTest REQUIRED)

add_executable(kv_test test/test_kv.cpp)

target_include_directories(kv_test PRIVATE
    ${CMAKE_SOURCE_DIR}/test
)

target_link_libraries(kv_test PRIVATE
    kvdb_lib
    GTest::GTest
    GTest::Main
)

# Platform-specific linker flags
if(UNIX)
    target_link_libraries(kv_test PRIVATE pthread)
endif(UNIX)

# Register with CTest
include(GoogleTest)
gtest_discover_tests(kv_test)

# --- Convenience targets ---
find_program(VALGRIND valgrind)
if(VALGRIND)
    add_custom_target(memcheck
        COMMAND ${VALGRIND} --leak-check=full $<TARGET_FILE:kv_test>
        DEPENDS kv_test
        COMMENT "Running Valgrind memory check"
    )
endif()

# Doxygen docs
find_program(DOXYGEN doxygen)
if(DOXYGEN)
    add_custom_target(doc
        COMMAND ${DOXYGEN} ${CMAKE_SOURCE_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating documentation"
    )
endif()
