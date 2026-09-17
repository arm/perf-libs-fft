# Workaround for GCC bug PR125795. Affected GCC point releases miscompile
# certain kernels - see below for affected files. When these releases are all
# available, prefer removing this workaround and instead require these versions
# if building SME2 ACLE kernels.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(ACLE_DISABLE_UNROLL_LOOPS OFF)
  if((CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "14"
      AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "14.4")
     OR (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "15"
         AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "15.4")
     OR (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "16"
         AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "16.2"))
    set(ACLE_DISABLE_UNROLL_LOOPS ON)
  endif()
endif()

target_sources(
  plfft_object PRIVATE ${CMAKE_CURRENT_LIST_DIR}/acle_sme2_kernel_data.cpp
                       ${CMAKE_CURRENT_LIST_DIR}/acle_sme2_kernel_lookup.cpp)

target_include_directories(
  plfft_object
  PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../../include
          ${CMAKE_CURRENT_LIST_DIR}/../.. ${CMAKE_CURRENT_LIST_DIR})

if(ENABLE_SME2)
  set(ACLE_SME2_SOURCES
      ${CMAKE_CURRENT_LIST_DIR}/sme2/plfft_256_sccnoh_tt.cpp
      ${CMAKE_CURRENT_LIST_DIR}/sme2/plfft_cccn_tt.cpp
      ${CMAKE_CURRENT_LIST_DIR}/sme2/plfft_cccn_tu.cpp
      ${CMAKE_CURRENT_LIST_DIR}/sme2/plfft_cccn_ut.cpp
      ${CMAKE_CURRENT_LIST_DIR}/sme2/plfft_cccn_uu.cpp
      ${CMAKE_CURRENT_LIST_DIR}/sme2/plfft_jjjn_tt.cpp
      ${CMAKE_CURRENT_LIST_DIR}/sme2/plfft_jjjn_tu.cpp
      ${CMAKE_CURRENT_LIST_DIR}/sme2/plfft_jjjn_ut.cpp
      ${CMAKE_CURRENT_LIST_DIR}/sme2/plfft_jjjn_uu.cpp)

  target_sources(plfft_object PRIVATE ${ACLE_SME2_SOURCES})

  if(APPLE)
    set(ACLE_SME2_COMPILE_OPTIONS -march=armv9-a+sme2+nosve)
  else()
    set(ACLE_SME2_COMPILE_OPTIONS -march=armv9-a+sme2)
  endif()
  set_source_files_properties(
    ${ACLE_SME2_SOURCES} PROPERTIES COMPILE_OPTIONS
                                    "${ACLE_SME2_COMPILE_OPTIONS}")

  # PR125795 is triggered by loop unrolling - add -fno-unroll-loops (takes
  # precedence over -funroll-loops as it appears later)
  if(ACLE_DISABLE_UNROLL_LOOPS)
    set_property(
      SOURCE ${CMAKE_CURRENT_LIST_DIR}/sme2/plfft_cccn_uu.cpp
             ${CMAKE_CURRENT_LIST_DIR}/sme2/plfft_jjjn_tt.cpp
      APPEND
      PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-fno-unroll-loops>")
  endif()

endif()
