cmake_minimum_required(VERSION 3.15)
project(sloejit)

# Only non-baremetal builds are currently tested, but baremetal has worked in
# the past. Mark SLOEJIT_BAREMETAL as advanced as it is not officially supported
option(SLOEJIT_BAREMETAL "Build for bare-metal" OFF)
mark_as_advanced(SLOEJIT_BAREMETAL)

add_compile_options(-Wextra -pedantic)

if(CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "MultiThreaded.*DLL")
  add_compile_definitions(_DLL)
endif()

enable_testing()
add_subdirectory(src)
set_target_properties(sloejit PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Provide the option to disable SloeJIT tests, as there are a lot of them and
# they take time to build. It is safe to disable this if you are not working on
# SloeJIT
option(SLOEJIT_ENABLE_TESTS "Enable SloeJIT unit tests" ON)
if(SLOEJIT_ENABLE_TESTS)
  add_subdirectory(test)
endif()
