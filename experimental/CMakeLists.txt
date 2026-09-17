if(NOT PLFFT_ENABLE_FIXED_POINT)
  message(
    FATAL_ERROR "Skipping experiments: fixed-point transforms are not enabled")
endif()

add_executable(dft_q0_7 dft_q0_7.cpp)
target_link_libraries(dft_q0_7 PRIVATE plfft)

option(DFT1D_ENABLE_DOTPROD "Enable dot product instructions for dft_q0_7" ON)
option(DFT1D_ENABLE_I8MM "Enable int8 matmul instructions for dft_q0_7" OFF)

set(DFT1D_MARCH "armv8.2-a+crypto+fp16")
if(DFT1D_ENABLE_DOTPROD)
  string(APPEND DFT1D_MARCH "+dotprod")
  add_compile_definitions(DFT1D_DOTPROD)
endif()
if(DFT1D_ENABLE_I8MM)
  string(APPEND DFT1D_MARCH "+i8mm")
  add_compile_definitions(DFT1D_I8MM)
endif()

target_compile_options(dft_q0_7 PRIVATE # $<$<CONFIG:Release>:-O3>
                                        -march=${DFT1D_MARCH})
add_test(NAME dft_q0_7 COMMAND $<TARGET_FILE:dft_q0_7>)
