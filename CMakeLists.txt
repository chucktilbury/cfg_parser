cmake_minimum_required(VERSION 3.18)
project(config)

#set(CMAKE_VERBOSE_MAKEFILE ON)
find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)

BISON_TARGET(PARSER parser.y ${CMAKE_CURRENT_BINARY_DIR}/parser.c COMPILE_FLAGS "--report=lookahead -tkv --name-prefix=cfg_")
FLEX_TARGET(SCANNER scanner.l  ${CMAKE_CURRENT_BINARY_DIR}/scanner.c COMPILE_FLAGS "-i --prefix=cfg_" )
ADD_FLEX_BISON_DEPENDENCY(SCANNER PARSER)

#set(BISON_PARSER_COMPILE_FLAG "--report=lookahead -tv --name-prefix=cfg")

include_directories( ${CMAKE_CURRENT_BINARY_DIR} ./ )

add_library( ${PROJECT_NAME}
    ${BISON_PARSER_OUTPUTS}
    ${FLEX_SCANNER_OUTPUTS}
    values.c
    errors.c
    config.c
    cmdline.c
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    $<$<CONFIG:DEBUG>:-g3>
    $<$<CONFIG:DEBUG>:-Og>
    $<$<CONFIG:DEBUG>:-DMEMORY_DEBUG>
    $<$<CONFIG:DEBUG>:-DENA_TRACE>
    $<$<CONFIG:RELEASE>:-Ofast>
    $<$<CONFIG:PROFILE>:-pg -O0>
)

set_property(DIRECTORY PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/parser.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/parser.output"
)

add_subdirectory(tests)
