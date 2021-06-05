
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

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -g -O0 -Wall -Wextra -DZDEBUG -Wno-unused-function -Wno-unneeded-internal-declaration")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O2 -Wall -Wextra")

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

bison_target(sysy_parser sysy.y ${CMAKE_CURRENT_BINARY_DIR}/sysy.tab.cpp
  DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/sysy.tab.hpp
  COMPILE_FLAGS -Wcounterexamples)

flex_target(sysy_lexer sysy.l ${CMAKE_CURRENT_BINARY_DIR}/sysy.lex.cpp)

add_flex_bison_dependency(sysy_lexer sysy_parser)

add_executable(sysy_eeyore
  ${BISON_sysy_parser_OUTPUTS} ${FLEX_sysy_lexer_OUTPUTS} sysy_bridge.cpp
  main.cpp
  eeyore_gen.cpp eeyore_dump.cpp
  eeyore_analysis.cpp
  tigger_gen.cpp tigger_dump.cpp)
