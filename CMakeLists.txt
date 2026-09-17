cmake_minimum_required(VERSION 3.15)
project(perf-libs-fft)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE
      Release
      CACHE STRING "Build type" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release MinSizeRel
                                               RelWithDebInfo)
endif()

# === Cache variables which we want to query during component creation ===
set(PLFFT_COMPILER_ID
    "${CMAKE_CXX_COMPILER_ID}"
    CACHE STRING "Compiler name")
mark_as_advanced(PLFFT_COMPILER_ID)
set(PLFFT_COMPILER_VERSION
    "${CMAKE_CXX_COMPILER_VERSION}"
    CACHE STRING "Compiler version")
mark_as_advanced(PLFFT_COMPILER_VERSION)

option(PLFFT_ENABLE_OPENMP "Build plfft with OpenMP support and _mp suffix" OFF)
if(PLFFT_ENABLE_OPENMP)
  set(MP_SUFFIX "_mp")
  set(OPENMP_FLAGS "-fopenmp")
else()
  set(MP_SUFFIX "")
  set(OPENMP_FLAGS "")
endif()

# === Option to enable code coverage ===
option(PLFFT_ENABLE_COVERAGE "Enable coverage testing" OFF)
if(PLFFT_ENABLE_COVERAGE)
  message(STATUS "Code coverage ENABLED")
  add_compile_options(--coverage)
  add_link_options(--coverage)
endif()

# === Define the Windows linkage to be /MD if not explicitly given ===
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  if(MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
      set(CMAKE_MSVC_RUNTIME_LIBRARY
          "MultiThreadedDLL"
          CACHE STRING "MSVC runtime library" FORCE)
    endif()
  endif()
endif()

function(validate_choice_var VAR_NAME)
  set(OPTIONS ${ARGN})
  if(NOT "${${VAR_NAME}}" IN_LIST OPTIONS)
    list(JOIN OPTIONS "', '" opts)
    message(
      FATAL_ERROR
        "Invalid ${VAR_NAME}='${${VAR_NAME}}'. Expected one of: '${opts}'.")
  endif()
  set_property(CACHE ${VAR_NAME} PROPERTY STRINGS ${OPTIONS})
endfunction()

# === Select backend provider ===
set(PLFFT_KERNEL_PROVIDER
    "aot"
    CACHE STRING "Which backend to use for FFT kernels (aot or jit)")
set(PLFFT_KERNEL_PROVIDER_OPTIONS aot jit)
validate_choice_var(PLFFT_KERNEL_PROVIDER ${PLFFT_KERNEL_PROVIDER_OPTIONS})
message(STATUS "Using kernel provider: ${PLFFT_KERNEL_PROVIDER}")

# === Only the AOT provider supports fixed-point transforms ===
if(PLFFT_KERNEL_PROVIDER STREQUAL "aot")
  set(PLFFT_ENABLE_FIXED_POINT ON)
  add_compile_definitions(PLFFT_ENABLE_FIXED_POINT)
else() # jit
  set(PLFFT_ENABLE_FIXED_POINT OFF)
endif()

# === Option to enable Arm64EC build for Windows only ===
option(PLFFT_ENABLE_ARM64EC
       "Enable Arm64EC ABI build (--target=arm64ec-pc-windows-msvc)" OFF)
if(PLFFT_ENABLE_ARM64EC)
  if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # We set the Arm64EC compiler flags when building the final library that
    # includes the kernels but when we build WFTA/Sloejit to generate the
    # kernels we must do it natively
    if(PLFFT_KERNEL_PROVIDER STREQUAL "aot")
      add_compile_options(--target=arm64ec-pc-windows-msvc)
      add_link_options(--target=arm64ec-pc-windows-msvc)
    endif()
    add_compile_definitions(PLFFT_ENABLE_ARM64EC)
    message(STATUS "Arm64EC build ENABLED")
  else()
    message(
      FATAL_ERROR
        "Option PLFFT_ENABLE_ARM64EC unsupported on non-Windows system")
  endif()
endif()

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# PLFFT_SME can have one of the following states.
#
# * Off: disable SME and SME2
# * Auto: detect both
# * SME: require SME1; SME2 optional
# * SME2: require both
set(PLFFT_SME
    "Auto"
    CACHE STRING "Build SME kernels (Off, Auto, SME, or SME2)")
validate_choice_var(PLFFT_SME Off Auto SME SME2)

if(PLFFT_SME STREQUAL "Off")
  set(ENABLE_SME OFF)
else()
  try_compile(
    ENABLE_SME ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/CheckSME
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sme.c
    COMPILE_DEFINITIONS -march=armv9-a+sme
    OUTPUT_VARIABLE TRY_COMPILE_SME_OUTPUT)
endif()

if(PLFFT_SME STREQUAL "Off")
  message(STATUS "SME is disabled by PLFFT_SME")
elseif(ENABLE_SME)
  message(STATUS "Compiler supports SME - including SME kernels")
  add_compile_definitions(PLFFT_ENABLE_SME)
elseif(PLFFT_SME STREQUAL "SME" OR PLFFT_SME STREQUAL "SME2")
  message(FATAL_ERROR "Could not build SME: ${TRY_COMPILE_SME_OUTPUT}")
else()
  message(STATUS "Compiler does not support SME - not including SME kernels")
endif()

set(ENABLE_SME_TESTS OFF)
if(ENABLE_SME)
  if(CMAKE_CROSSCOMPILING AND NOT CMAKE_CROSSCOMPILING_EMULATOR)
    set(TRY_RUN_SME_OUTPUT
        "Cannot run the SME probe while cross-compiling without CMAKE_CROSSCOMPILING_EMULATOR"
    )
  else()
    try_run(
      TRY_RUN_SME_RESULT TRY_RUN_SME_COMPILED
      ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/CheckSMERun
      ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sme.c
      COMPILE_DEFINITIONS -march=armv9-a+sme
      RUN_OUTPUT_VARIABLE TRY_RUN_SME_OUTPUT
      COMPILE_OUTPUT_VARIABLE TRY_RUN_SME_COMPILE_OUTPUT)
    if(TRY_RUN_SME_COMPILED AND TRY_RUN_SME_RESULT EQUAL 0)
      set(ENABLE_SME_TESTS ON)
    endif()
  endif()

  if(ENABLE_SME_TESTS)
    message(STATUS "System supports SME - enabling SME tests")
  elseif(PLFFT_SME STREQUAL "SME" OR PLFFT_SME STREQUAL "SME2")
    message(
      FATAL_ERROR
        "SME is required, but the SME probe could not be run successfully. "
        "Compile output: ${TRY_RUN_SME_COMPILE_OUTPUT}\n"
        "Run output: ${TRY_RUN_SME_OUTPUT}")
  else()
    message(STATUS "System does not support SME - not enabling SME tests")
  endif()
endif()

if(NOT ENABLE_SME)
  set(ENABLE_SME2 OFF)
else()
  try_compile(
    ENABLE_SME2 ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/CheckSME2
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sme2.cpp
    COMPILE_DEFINITIONS -march=armv9-a+sme2
    OUTPUT_VARIABLE TRY_COMPILE_SME2_OUTPUT)
endif()

if(PLFFT_SME STREQUAL "Off")
  message(STATUS "SME2 is disabled by PLFFT_SME")
elseif(ENABLE_SME2)
  message(STATUS "Compiler supports SME2 - including SME2 kernels")
  add_compile_definitions(PLFFT_ENABLE_SME2)
elseif(PLFFT_SME STREQUAL "SME2")
  message(FATAL_ERROR "Could not build SME2: ${TRY_COMPILE_SME2_OUTPUT}")
else()
  message(STATUS "Compiler does not support SME2 - not including SME2 kernels")
endif()

# === AddressSanitizer toggle ===
option(PLFFT_ENABLE_ASAN "Enable AddressSanitizer (-fsanitize=address)" OFF)

if(PLFFT_ENABLE_ASAN)
  message(STATUS "AddressSanitizer ENABLED")
  add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address)
endif()

# === THREAD_SAFE toggle ===
# thread_local and -fthreadsafe-statics aren't supported by all toolchains, so
# allow them to be disabled if necessary. We typically can't support
# -fthreadsafe-statics, because it requires C++ runtime support, but keeping it
# enabled results in useful linker errors when using an unsafe static variable.
# thread_local implies static storage, so static is a reasonable replacement
# when thread_local isn't supported.
option(PLFFT_THREAD_SAFE "Enable use of thread_local storage" ON)
if(NOT PLFFT_THREAD_SAFE)
  add_compile_definitions(THREAD_LOCAL=static)
  add_compile_options(-fno-threadsafe-statics)
endif()

# === C++ standard ===
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# === Custom compile flags ===
add_compile_options(
  -ffp-contract=fast
  -Wuninitialized
  -fno-exceptions
  $<$<CONFIG:Release>:-funroll-loops>
  -Wall
  -Werror
  # Only warn for shadowing for static kernels - shadowing is too prevalent in
  # sloejit to be easily fixed
  $<$<STREQUAL:${PLFFT_KERNEL_PROVIDER},aot>:-Wshadow>
  -Wno-missing-braces
  -Wno-error=unknown-pragmas
  # assert on Windows can continue after failure, so compiler cannot prove paths
  # are unreachable after assert(false)
  $<$<PLATFORM_ID:Windows>:-Wno-error=return-type>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
  $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:GNU>>:-fconcepts>)

# === Detect OS and cache for querying during component creation ===
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Android")
  set(PLFFT_TARGET_OS
      "linux"
      CACHE STRING "Normalized build OS name")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(PLFFT_TARGET_OS
      "mac"
      CACHE STRING "Normalized build OS name")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(PLFFT_TARGET_OS
      "windows"
      CACHE STRING "Normalized build OS name")
  add_compile_definitions(
    _SILENCE_NONFLOATING_COMPLEX_DEPRECATION_WARNING _CRT_SECURE_NO_WARNINGS
    _CRT_NONSTDC_NO_WARNINGS _CRT_USE_C_COMPLEX_H NOMINMAX)
else()
  message(FATAL_ERROR "Unsupported OS: ${CMAKE_SYSTEM_NAME}")
endif()
mark_as_advanced(PLFFT_TARGET_OS)

# For portability of builds with static kernels, prefer no C++ runtime
if(PLFFT_KERNEL_PROVIDER STREQUAL "aot" AND NOT PLFFT_ENABLE_ARM64EC)
  add_compile_options(-nostdlib)
  add_compile_definitions(NO_LIBCPP=1 _GLIBCXX_NO_ASSERTIONS)
endif()

include(ExternalProject)

# === Explicit source files ===
set(SRC_CPP_FILES
    src/cpu_features.cpp
    src/arm_fft1d.cpp
    src/compositor.cpp
    src/exec_compositor.cpp
    src/exec_convolution.cpp
    src/fft_buffers.cpp
    src/fft_config.cpp
    src/r2r_plan.cpp
    src/rader.cpp
    src/rader_generator.cpp
    src/bluestein.cpp
    src/level_ab_n.cpp
    src/level_ab_t_c2c.cpp
    src/level_abx_t_c2c.cpp
    src/level_ab_t_c2r.cpp
    src/level_ab_t_r2c.cpp
    src/level_ac_t_c2c.cpp
    src/level_ac_t_c2r.cpp
    src/level_ac_t_r2c.cpp
    src/plfft_statistics.cpp
    src/planner/benchmarker.cpp
    src/planner/planner.cpp
    src/planner/wisdom.cpp
    src/plfft.cpp
    src/vector_size.cpp)

# === If provider is JIT, include its files explicitly ===
if(PLFFT_KERNEL_PROVIDER STREQUAL "jit")
  list(
    APPEND
    SRC_CPP_FILES
    src/providers/jit/algo.cpp
    src/providers/jit/clean.cpp
    src/providers/jit/expr.cpp
    src/providers/jit/io_pointers.cpp
    src/providers/jit/irbuilder.cpp
    src/providers/jit/irprinter.cpp
    src/providers/jit/irvalue.cpp
    src/providers/jit/irvalue_scope.cpp
    src/providers/jit/irvalue_scope_iterator.cpp
    src/providers/jit/kernel_cache.cpp
    src/providers/jit/jit_kernel_data.cpp
    src/providers/jit/kernel_printer.cpp
    src/providers/jit/print_algo.cpp
    src/providers/jit/print_value.cpp
    src/providers/jit/radix5_z25.cpp
    src/providers/jit/type_check.cpp
    src/providers/jit/wfta_nfact.cpp
    src/providers/jit/winograd_reorder.cpp
    src/providers/jit/winograd_z2_separated.cpp
    src/providers/jit/winograd_z3_separated.cpp
    src/providers/jit/winograd_z4_separated.cpp
    src/providers/jit/winograd_z5_separated.cpp
    src/providers/jit/winograd_z7_separated.cpp
    src/providers/jit/winograd_z8_separated.cpp
    src/providers/jit/winograd_z9_separated.cpp
    src/providers/jit/winograd_z11_separated.cpp
    src/providers/jit/winograd_z13_separated.cpp
    src/providers/jit/winograd_z16_separated.cpp
    src/providers/jit/winograd_z17_separated.cpp
    src/providers/jit/winograd_z19_separated.cpp
    src/providers/jit/split_radix_z16.cpp
    src/providers/jit/split_radix_z32.cpp)
elseif(PLFFT_KERNEL_PROVIDER STREQUAL "aot")
  list(APPEND SRC_CPP_FILES src/providers/aot/aot_kernel_data.cpp
       src/providers/aot/aot_kernel_lookup.cpp src/providers/aot/clean.cpp)
  # If we are building this kernel provider, then it's implied we want to avoid
  # a dependence on libstdc++. This requires resolving the __cxa_thread_atexit
  # symbols emitted as per the Linux C++ ABI. but which is provided by
  # libstdc++. Happily, we can provide our own version of this symbol which
  # calls __cxa_thread_atexit_impl which is in libc. Additionally we must
  # provide an implementation of __cxa_pure_virtual.
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux"
     OR CMAKE_SYSTEM_NAME STREQUAL "Darwin"
     OR CMAKE_SYSTEM_NAME STREQUAL "Android")
    list(APPEND SRC_CPP_FILES src/providers/aot/cxa_shims.cpp)
  endif()
endif()

# === Create object library ===
add_library(plfft_object OBJECT ${SRC_CPP_FILES})
set_target_properties(plfft_object PROPERTIES POSITION_INDEPENDENT_CODE ON)

# === Define machine_static from machine sources ===
if(PLFFT_KERNEL_PROVIDER STREQUAL "jit")
  set(MACHINE_SOURCES machine/get_sve.cpp)

  add_library(machine_static STATIC ${MACHINE_SOURCES})
  target_include_directories(
    machine_static
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/machine>)
  target_include_directories(machine_static
                             PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
  set_target_properties(machine_static PROPERTIES OUTPUT_NAME "machine")

  target_link_libraries(plfft_object PRIVATE machine_static)
endif()

target_include_directories(plfft_object PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(plfft_object
                           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_options(plfft_object PUBLIC ${OPENMP_FLAGS})

if(PLFFT_KERNEL_PROVIDER STREQUAL "jit")
  add_subdirectory(sloejit)
  target_link_libraries(plfft_object PRIVATE sloejit)
  # For JIT the base architecture for support code is armv8-a, meaning any
  # armv8-a target can run the library but FP16 arithmetic will not be used
  # outside FP16 kernels (so codegen for e.g. convolution plans may be
  # suboptimal)
  if(NOT PLFFT_ENABLE_ARM64EC)
    set(SRC_ARCH_COMPILE_OPTIONS -march=armv8-a)
  endif()
elseif(PLFFT_KERNEL_PROVIDER STREQUAL "aot")
  # AOT has stricter FP16 support - if FP16 is not present an FP16 plan will be
  # refused. This means we can enable FP16 as the base arch
  set(SRC_ARCH_COMPILE_OPTIONS -march=armv8-a+fp16)
  # Add assembly kernels to plfft_object.
  include(${CMAKE_CURRENT_SOURCE_DIR}/src/providers/aot/kernels.cmake)
endif()

set_property(
  SOURCE ${SRC_CPP_FILES}
  APPEND
  PROPERTY COMPILE_OPTIONS ${OPENMP_FLAGS} ${SRC_ARCH_COMPILE_OPTIONS})

include(${CMAKE_CURRENT_SOURCE_DIR}/src/providers/acle/acle.cmake)

# === Final libraries using object files ===
add_library(plfft STATIC)
target_sources(plfft PRIVATE $<TARGET_OBJECTS:plfft_object>)
target_include_directories(
  plfft PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
               $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_options(plfft PUBLIC ${OPENMP_FLAGS})

option(PLFFT_STATIC_TESTING "Link tests and benchmarks statically" OFF)
if(PLFFT_STATIC_TESTING)
  target_link_libraries(plfft INTERFACE -static)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Android")
  target_link_libraries(plfft INTERFACE m)
endif()

if(PLFFT_KERNEL_PROVIDER STREQUAL "jit")
  target_link_libraries(plfft INTERFACE "$<LINK_ONLY:sloejit>"
                                        "$<LINK_ONLY:machine_static>")
endif()

if(ENABLE_SME)
  if(CMAKE_C_COMPILER_ID MATCHES ".*Clang.*")
    target_link_options(plfft INTERFACE --rtlib=compiler-rt)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      target_link_options(plfft INTERFACE -fuse-ld=lld -lunwind)
    endif()
  endif()
endif()

if(CMAKE_MSVC_RUNTIME_LIBRARY STREQUAL "MultiThreaded")
  set(PLFFT_OUTPUT_NAME "libplfft${MP_SUFFIX}")
else() # matches "MultiThreadedDLL" and also sets correctly for linux/mac where
       # "lib" is automatically prepended
  set(PLFFT_OUTPUT_NAME "plfft${MP_SUFFIX}")
endif()
set_target_properties(
  plfft
  PROPERTIES
    OUTPUT_NAME "${PLFFT_OUTPUT_NAME}"
    EXPORT_NAME plfft
    PUBLIC_HEADER
    "${CMAKE_CURRENT_SOURCE_DIR}/include/arm_fft1d.hpp;${CMAKE_CURRENT_SOURCE_DIR}/include/plfft.h"
)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PLFFTConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/PLFFTConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/plfft")

# === Install rules ===
set(PLFFT_EXPORT_TARGETS plfft)
if(PLFFT_KERNEL_PROVIDER STREQUAL "jit")
  list(APPEND PLFFT_EXPORT_TARGETS sloejit machine_static)
endif()
install(TARGETS ${PLFFT_EXPORT_TARGETS} EXPORT PLFFTTargets)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/PLFFTConfig.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/plfft")
install(
  EXPORT PLFFTTargets
  FILE PLFFTTargets.cmake
  NAMESPACE PLFFT::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/plfft")
install(DIRECTORY include/plfft TYPE INCLUDE)
install(
  FILES "${CMAKE_CURRENT_SOURCE_DIR}/COPYRIGHT.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSES/MIT.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSES/Apache-2.0.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSES/LLVM-exception.txt"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/plfft")

# === Static kernel generator ===
if(PLFFT_KERNEL_PROVIDER STREQUAL "jit")
  add_subdirectory(scripts/gen_asm_kernels)
endif()

option(PLFFT_BUILD_BENCHMARKS "Build benchmark executable" ON)
if(PLFFT_BUILD_BENCHMARKS)
  add_executable(bench bench/bench.cpp)
  target_link_libraries(bench PRIVATE plfft)
  target_include_directories(
    bench PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/bench/include")

  # Generate benchmark driver launcher in the build directory so that benchmarks
  # can be invoked easily from build without copying Python helper modules.
  set(PLFFT_BENCH_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/bench/bench_driver.py.in"
                 "${CMAKE_CURRENT_BINARY_DIR}/bench_driver.py" @ONLY)
endif()

option(PLFFT_BUILD_EXPERIMENTAL "Build experimental executables and tests" OFF)

# Utility function to add a test
function(add_fft_test TEST_NAME)
  # Build the actual test executable itself
  add_executable(${TEST_NAME} test/${TEST_NAME}.cpp)
  target_link_libraries(${TEST_NAME} PRIVATE plfft)
  if(PLFFT_KERNEL_PROVIDER STREQUAL "jit")
    # Some layouts are not supported by the static kernels - we can add more
    # tests if we are testing the JIT backend
    target_compile_definitions(${TEST_NAME} PRIVATE TEST_JIT_ONLY_CONFIGS)
  endif()
  # Register it as a test, set up dependencies
  add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
  # ----- macOS-only: compute <toolchain>/lib and wire it up -----
  if(APPLE)
    # Resolve: <toolchain>/bin/<compiler>  -->  <toolchain>/lib
    get_filename_component(_cmp_real "${CMAKE_CXX_COMPILER}" REALPATH)
    get_filename_component(_bin_dir "${_cmp_real}" DIRECTORY) # .../bin
    get_filename_component(_toolroot "${_bin_dir}/.." REALPATH) # parent of bin
    set(_tool_lib "${_toolroot}/lib")

    # Set DYLD_LIBRARY_PATH for ctest-run of this test Use APPEND to avoid
    # clobbering any existing ENV for this test
    set_property(
      TEST ${TEST_NAME}
      APPEND
      PROPERTY ENVIRONMENT
               "DYLD_LIBRARY_PATH=${_tool_lib}:$ENV{DYLD_LIBRARY_PATH}")
  endif()

endfunction()

if(NOT PLFFT_ENABLE_ARM64EC)
  enable_testing()

  if(PLFFT_KERNEL_PROVIDER STREQUAL "jit" AND CMAKE_SYSTEM_NAME STREQUAL
                                              "Linux")
    add_test(
      NAME check_archive
      COMMAND
        ${Python3_EXECUTABLE}
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_archive.py"
        $<TARGET_FILE:plfft> $<TARGET_FILE:sloejit>)
  endif()

  add_fft_test(simple_test)
  add_fft_test(c2c_f16)
  add_fft_test(c2c_f32)
  add_fft_test(c2c_f64)
  add_fft_test(constant_to_delta_f16)
  add_fft_test(constant_to_delta_f32)
  add_fft_test(constant_to_delta_f64)
  add_fft_test(delta_to_constant_f16)
  add_fft_test(delta_to_constant_f32)
  add_fft_test(delta_to_constant_f64)
  add_fft_test(plfft_interface_test)
  add_fft_test(plfft_may_alias_test)
  add_fft_test(pure_tone_fwd_f16)
  add_fft_test(pure_tone_fwd_f32)
  add_fft_test(pure_tone_fwd_f64)
  add_fft_test(r2c_c2r_1d_batched)
  add_fft_test(r2r_test)

  if(ENABLE_SME_TESTS)
    add_fft_test(simple_test_sme)
    add_fft_test(c2c_f16_sme)
    add_fft_test(c2c_f32_sme)
    add_fft_test(c2c_f64_sme)
    add_fft_test(constant_to_delta_f16_sme)
    add_fft_test(constant_to_delta_f32_sme)
    add_fft_test(constant_to_delta_f64_sme)
    add_fft_test(delta_to_constant_f16_sme)
    add_fft_test(delta_to_constant_f32_sme)
    add_fft_test(delta_to_constant_f64_sme)
    add_fft_test(numerical_reference_sme)
    add_fft_test(pure_tone_fwd_f16_sme)
    add_fft_test(pure_tone_fwd_f32_sme)
    add_fft_test(pure_tone_fwd_f64_sme)
    add_fft_test(r2c_f32_sme)
    add_fft_test(plfft_sme_not_available_test)
    set_property(
      TEST plfft_sme_not_available_test
      APPEND
      PROPERTY ENVIRONMENT "PLFFT_DISABLE_SME=1")
  endif()

  if(PLFFT_ENABLE_FIXED_POINT)
    add_fft_test(c2c_q0_7)
    add_fft_test(constant_to_delta_q0_7)
    add_fft_test(delta_to_constant_q0_7)
    add_fft_test(pure_tone_fwd_q0_7)
    add_fft_test(c2c_q0_15)
    add_fft_test(constant_to_delta_q0_15)
    add_fft_test(delta_to_constant_q0_15)
    add_fft_test(pure_tone_fwd_q0_15)
  endif()

  if(PLFFT_BUILD_EXPERIMENTAL)
    add_subdirectory(experimental)
  endif()

  if(PLFFT_KERNEL_PROVIDER STREQUAL "aot")
    add_subdirectory(test/c_compat)
  endif()
endif()
