enable_language(ASM)

set(MAKO_GEN ${CMAKE_CURRENT_SOURCE_DIR}/mako_gen.py)

function(add_mako_generated_file OUT_REL)
  set(EXTRA_DEPS ${ARGN})
  set(IN_ABS ${CMAKE_CURRENT_SOURCE_DIR}/${OUT_REL}.mako)
  set(OUT_ABS ${CMAKE_CURRENT_BINARY_DIR}/${OUT_REL})
  get_filename_component(DIR ${OUT_REL} DIRECTORY)
  add_custom_command(
    OUTPUT ${OUT_ABS}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${DIR}
    COMMAND ${MAKO_GEN} -o ${OUT_ABS} ${IN_ABS}
    DEPENDS ${MAKO_GEN} ${IN_ABS} ${EXTRA_DEPS}
    VERBATIM)
endfunction()

add_mako_generated_file(include/sloejit/aarch64/aarch64.hpp)
add_mako_generated_file(include/sloejit/aarch64/aarch64_opcodes.hpp
                        include/sloejit/aarch64/aarch64_opcodes.py)
add_mako_generated_file(
  aarch64/aarch64.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/include/sloejit/aarch64/aarch64.hpp
  ${CMAKE_CURRENT_BINARY_DIR}/include/sloejit/aarch64/aarch64_opcodes.hpp)

add_library(
  sloejit STATIC
  ${CMAKE_CURRENT_BINARY_DIR}/aarch64/aarch64.cpp
  plfft_clear_icache.S
  block.cpp
  function.cpp
  gtelf_internal.cpp
  instruction.cpp
  ir.cpp
  util.cpp)
if(SLOEJIT_BAREMETAL)
  # For Baremetal builds only, set the number of instructions we paste into our
  # "host" function
  target_sources(sloejit PRIVATE jit_host.S)
  add_compile_definitions(BM_HOST_INS=1000000)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_sources(sloejit PRIVATE gdb.cpp jit_debug_register_code.cpp)
endif()

target_include_directories(
  sloejit PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                 $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>)
target_include_directories(sloejit
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/sloejit)
