# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

get_filename_component(basename ${CMAKE_CURRENT_SOURCE_DIR} NAME)
project(test_${basename}
    VERSION 1.0.0
    LANGUAGES C)


string(REGEX REPLACE
    "/test/bacnet/[a-zA-Z_/-]*$"
    "/src"
    SRC_DIR
    ${CMAKE_CURRENT_SOURCE_DIR})
string(REGEX REPLACE
    "/test/bacnet/[a-zA-Z_/-]*$"
    "/test"
    TST_DIR
    ${CMAKE_CURRENT_SOURCE_DIR})
set(ZTST_DIR "${TST_DIR}/ztest/src")

add_compile_definitions(
    MAX_APDU=1476
    CONFIG_ZTEST=1
    )

include_directories(
    ${SRC_DIR}
    ${TST_DIR}/ztest/include
    )

add_executable(${PROJECT_NAME}
    # File(s) under test
    ${SRC_DIR}/bacnet/datalink/cobs.c
    # Support files and stubs (pathname alphabetical)
    # Test and test library files
    ./src/main.c
    ${ZTST_DIR}/ztest_mock.c
    ${ZTST_DIR}/ztest.c
    )
