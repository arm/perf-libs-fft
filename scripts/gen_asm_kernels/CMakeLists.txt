# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates
# <open-source-office@arm.com>
#
# SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_executable(gen_asm)
target_sources(gen_asm PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/gen_asm.cpp"
                               "${CMAKE_CURRENT_SOURCE_DIR}/parse.cpp")
target_include_directories(
  gen_asm PRIVATE "${PROJECT_SOURCE_DIR}/src"
                  $<TARGET_PROPERTY:sloejit,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(gen_asm PRIVATE plfft)

add_custom_target(
  gen_asm_kernels
  COMMAND
    ${CMAKE_COMMAND} -E env PYTHONPATH=${PROJECT_SOURCE_DIR}
    ${Python3_EXECUTABLE} -m scripts.gen_asm_kernels.main $<TARGET_FILE:gen_asm>
    ${PROJECT_SOURCE_DIR}/src/providers/aot
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/main.py"
          "${CMAKE_CURRENT_SOURCE_DIR}/config.py"
          "${CMAKE_CURRENT_SOURCE_DIR}/gen_lookup.py"
          "${CMAKE_CURRENT_SOURCE_DIR}/targets.py"
          gen_asm
  WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  COMMENT "Generating assembly kernel source files"
  VERBATIM)

add_custom_target(
  check_asm_kernels
  COMMAND
    ${CMAKE_COMMAND} -E env PYTHONPATH=${PROJECT_SOURCE_DIR}
    ${Python3_EXECUTABLE} -m scripts.gen_asm_kernels.main $<TARGET_FILE:gen_asm>
    ${PROJECT_SOURCE_DIR}/src/providers/aot --check-only
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/main.py"
          "${CMAKE_CURRENT_SOURCE_DIR}/config.py"
          "${CMAKE_CURRENT_SOURCE_DIR}/gen_lookup.py"
          "${CMAKE_CURRENT_SOURCE_DIR}/targets.py"
          gen_asm
  WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  COMMENT "Checking if assembly kernel source files have changed"
  VERBATIM)
