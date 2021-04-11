
# WARNING: For local compilation only!
# According to course instruction, the online judge website won't
# use Makefile or CMake. Instead, it scans all source files, and then
# preprocess, compile, and link them "heuristically". It is one's own
# responsibility to make these "heuristics" work.
# This CMake project is for one's reference.

cmake_minimum_required(VERSION 3.10)
project(compiler101)

find_package(BISON)
find_package(FLEX)

# flex_target(sysy_parser sysy.l)
bison_target(sysy_parser sysy.y ${CMAKE_CURRENT_BINARY_DIR}/sysy.tab.c
  DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/sysy.tab.h
  COMPILE_FLAGS -Wcounterexamples)
add_executable(sysy_test ${BISON_sysy_parser_OUTPUTS})
