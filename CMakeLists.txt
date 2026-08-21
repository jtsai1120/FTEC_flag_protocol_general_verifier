cmake_minimum_required(VERSION 3.20)
project(fpdl_parser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(fpdl_parser
    src/fpdl_parser.cpp
)
target_include_directories(fpdl_parser PUBLIC include)
target_compile_options(fpdl_parser PRIVATE -Wall -Wextra -Wpedantic)

add_executable(fpdlc src/main.cpp)
target_link_libraries(fpdlc PRIVATE fpdl_parser)

include(CTest)
if(BUILD_TESTING)
    add_executable(fpdl_parser_tests tests/parser_tests.cpp)
    target_link_libraries(fpdl_parser_tests PRIVATE fpdl_parser)

    file(GLOB FPDL_EXAMPLES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/*/*.fpdl")
    add_test(NAME parser_examples
             COMMAND fpdl_parser_tests ${FPDL_EXAMPLES})
endif()
