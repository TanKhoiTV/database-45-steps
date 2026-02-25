cmake_minimum_required(VERSION 3.20)

project(kvdb
    VERSION             0.6.0
    DESCRIPTION         "File-backed key-value store with sequential log"
    LANGUAGES           CXX
)

enable_testing()
list(APPEND CMAKE_CTEST_ARGUMENTS "--output-on-failure")

# --- Standard ---
set(CMAKE_CXX_STANDARD 23)              # C++23 standard
set(CMAKE_CXX_STANDARD_REQUIRED ON)     # error if compiler can't do C++23
set(CMAKE_CXX_EXTENSIONS OFF)           # use -std=c++23, not -std=gnu++23

# --- Version header ---
configure_file(
    ${CMAKE_SOURCE_DIR}/include/version.h.in
    ${CMAKE_BINARY_DIR}/include/version.h
)

# --- Library sources ---
set(LIB_SOURCES
    src/entry_codec.cpp
    src/log.cpp
    src/kv.cpp
)

if(UNIX)
    list(APPEND LIB_SOURCES src/platform_unix.cpp)
elseif(WIN32)
    list(APPEND LIB_SOURCES src/platform_windows.cpp)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# --- Build the library ---
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

# --- Dependencies ---
find_package(Threads REQUIRED)

set(FETCHCONTENT_QUIET OFF)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL      https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
    URL_HASH SHA256=40d4ec942217dcc84a9ebe2a68584ada7d4a33a8ee958755763278ea1c5e18ff
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googletest)

# --- Test executable ---
add_executable(kv_test test/test_kv.cpp)

target_include_directories(kv_test PRIVATE
    ${CMAKE_SOURCE_DIR}/test
)

target_link_libraries(kv_test PRIVATE
    kvdb_lib
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

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

find_program(DOXYGEN doxygen)
if(DOXYGEN)
    add_custom_target(doc
        COMMAND ${DOXYGEN} ${CMAKE_SOURCE_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating documentation"
    )
endif()
