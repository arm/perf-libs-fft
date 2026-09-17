cmake_minimum_required(VERSION 3.21)
project(perflibs_fft_examples LANGUAGES C CXX)

find_package(PLFFT CONFIG REQUIRED)

set(plfft_example_targets plfft_c2c_example plfft_r2c_example plfft_c2r_example
                          plfft_r2r_example)

add_custom_target(run_examples)

foreach(plfft_example_target IN LISTS plfft_example_targets)
  add_executable(${plfft_example_target} ${plfft_example_target}.c)
  target_link_libraries(${plfft_example_target} PRIVATE PLFFT::plfft)
  add_custom_target(run_${plfft_example_target} COMMAND ${plfft_example_target})
  add_dependencies(run_${plfft_example_target} ${plfft_example_target})
  add_dependencies(run_examples run_${plfft_example_target})
endforeach()
