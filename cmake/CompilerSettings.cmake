# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

include_guard(GLOBAL)

# C++ Standard Configuration
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# C Standard Configuration
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Position Independent Code
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Interprocedural Optimization (LTO)
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
  include(CheckIPOSupported)
  check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT IPO_ERROR)
  if(IPO_SUPPORTED)
    message(STATUS "Interprocedural optimization (LTO) enabled")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
  else()
    message(STATUS "Interprocedural optimization (LTO) not supported: ${IPO_ERROR}")
  endif()
endif()

# Compiler identification
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")

# Minimum compiler versions
set(GCC_MIN_VERSION "11.1.0")
set(CLANG_MIN_VERSION "14.0.0")
set(APPLE_CLANG_MIN_VERSION "14.0.0")
set(MSVC_MIN_VERSION "19.32")  # Visual Studio 2022

# Version checks
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS GCC_MIN_VERSION)
    message(FATAL_ERROR "GCC version ${CMAKE_CXX_COMPILER_VERSION} is too old. Minimum required: ${GCC_MIN_VERSION}")
  endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS CLANG_MIN_VERSION)
    message(FATAL_ERROR "Clang version ${CMAKE_CXX_COMPILER_VERSION} is too old. Minimum required: ${CLANG_MIN_VERSION}")
  endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS APPLE_CLANG_MIN_VERSION)
    message(FATAL_ERROR "Apple Clang version ${CMAKE_CXX_COMPILER_VERSION} is too old. Minimum required: ${APPLE_CLANG_MIN_VERSION}")
  endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS MSVC_MIN_VERSION)
    message(FATAL_ERROR "MSVC version ${CMAKE_CXX_COMPILER_VERSION} is too old. Minimum required: ${MSVC_MIN_VERSION}")
  endif()
endif()

# Global compiler options interface
add_library(trinity-compiler-options INTERFACE)
add_library(Trinity::CompilerOptions ALIAS trinity-compiler-options)

# Platform-specific settings
if(WIN32)
  target_compile_definitions(trinity-compiler-options INTERFACE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
    _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
  )
  
  # Windows Unicode support
  target_compile_definitions(trinity-compiler-options INTERFACE
    UNICODE
    _UNICODE
  )
endif()

# Architecture-specific optimizations
include(CheckCXXCompilerFlag)

# SSE2 support (required)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64|AMD64|amd64)")
  target_compile_definitions(trinity-compiler-options INTERFACE HAVE_SSE2 __SSE2__)
  if(NOT MSVC)
    target_compile_options(trinity-compiler-options INTERFACE -msse2)
  endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i686")
  check_cxx_compiler_flag("-msse2" HAS_SSE2_FLAG)
  if(HAS_SSE2_FLAG)
    target_compile_options(trinity-compiler-options INTERFACE -msse2 -mfpmath=sse)
    target_compile_definitions(trinity-compiler-options INTERFACE HAVE_SSE2 __SSE2__)
  endif()
endif()

# ARM-specific optimizations
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|ARM|aarch64|AARCH64)")
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "64")
    target_compile_definitions(trinity-compiler-options INTERFACE ARM64)
  else()
    target_compile_definitions(trinity-compiler-options INTERFACE ARM32)
  endif()
endif()

# Warning levels
add_library(trinity-warning-default INTERFACE)
add_library(trinity-warning-extra INTERFACE)
add_library(trinity-warning-all INTERFACE)
add_library(trinity-no-warning INTERFACE)

# Build configuration options
add_library(trinity-debug-options INTERFACE)
add_library(trinity-release-options INTERFACE)

# Sanitizer options
add_library(trinity-asan-options INTERFACE)
add_library(trinity-tsan-options INTERFACE)
add_library(trinity-ubsan-options INTERFACE)
add_library(trinity-msan-options INTERFACE)

# Compiler-specific configurations
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  include(${CMAKE_CURRENT_LIST_DIR}/compiler/GCC.cmake)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  include(${CMAKE_CURRENT_LIST_DIR}/compiler/Clang.cmake)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  include(${CMAKE_CURRENT_LIST_DIR}/compiler/MSVC.cmake)
endif()

# Default warnings
if(MSVC)
  target_compile_options(trinity-warning-default INTERFACE /W3)
  target_compile_options(trinity-warning-extra INTERFACE /W4)
  target_compile_options(trinity-warning-all INTERFACE /Wall)
  target_compile_options(trinity-no-warning INTERFACE /W0)
else()
  target_compile_options(trinity-warning-default INTERFACE -Wall)
  target_compile_options(trinity-warning-extra INTERFACE -Wall -Wextra)
  target_compile_options(trinity-warning-all INTERFACE 
    -Wall -Wextra -Wpedantic
    -Wshadow -Wnon-virtual-dtor
    -Wold-style-cast -Wcast-align
    -Wunused -Woverloaded-virtual
    -Wconversion -Wsign-conversion
    -Wmisleading-indentation
    -Wduplicated-cond
    -Wduplicated-branches
    -Wlogical-op
    -Wnull-dereference
    -Wuseless-cast
    -Wdouble-promotion
    -Wformat=2
  )
  target_compile_options(trinity-no-warning INTERFACE -w)
endif()

# Debug configurations
target_compile_definitions(trinity-debug-options INTERFACE
  $<$<CONFIG:Debug>:_DEBUG>
  $<$<CONFIG:Debug>:TRINITY_DEBUG>
)

if(NOT MSVC)
  target_compile_options(trinity-debug-options INTERFACE
    $<$<CONFIG:Debug>:-g3>
    $<$<CONFIG:Debug>:-ggdb>
    $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
  )
endif()

# Release configurations
target_compile_definitions(trinity-release-options INTERFACE
  $<$<CONFIG:Release>:NDEBUG>
)

if(NOT MSVC)
  target_compile_options(trinity-release-options INTERFACE
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Release>:-march=native>
    $<$<CONFIG:RelWithDebInfo>:-O2>
    $<$<CONFIG:RelWithDebInfo>:-g>
  )
else()
  target_compile_options(trinity-release-options INTERFACE
    $<$<CONFIG:Release>:/O2>
    $<$<CONFIG:Release>:/GL>
  )
endif()

# Sanitizer configurations

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  # Address Sanitizer
  target_compile_options(trinity-asan-options INTERFACE
    -fsanitize=address
    -fno-omit-frame-pointer
    -fno-optimize-sibling-calls
  )
  target_link_options(trinity-asan-options INTERFACE
    -fsanitize=address
  )
  
  # Thread Sanitizer
  target_compile_options(trinity-tsan-options INTERFACE
    -fsanitize=thread
    -fno-omit-frame-pointer
  )
  target_link_options(trinity-tsan-options INTERFACE
    -fsanitize=thread
  )
  
  # Undefined Behavior Sanitizer
  target_compile_options(trinity-ubsan-options INTERFACE
    -fsanitize=undefined
    -fno-omit-frame-pointer
  )
  target_link_options(trinity-ubsan-options INTERFACE
    -fsanitize=undefined
  )
  
  # Memory Sanitizer (Clang only)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(trinity-msan-options INTERFACE
      -fsanitize=memory
      -fno-omit-frame-pointer
      -fsanitize-memory-track-origins
    )
    target_link_options(trinity-msan-options INTERFACE
      -fsanitize=memory
    )
  endif()
endif()

# Unity build configuration
if(CMAKE_UNITY_BUILD)
  set(CMAKE_UNITY_BUILD_BATCH_SIZE 16 CACHE STRING "Unity build batch size")
  message(STATUS "Unity build enabled with batch size: ${CMAKE_UNITY_BUILD_BATCH_SIZE}")
endif()

# Precompiled headers configuration
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
  set(CMAKE_PCH_INSTANTIATE_TEMPLATES ON)
  set(CMAKE_PCH_WARN_INVALID ON)
endif()

# Export compile commands by default
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Color diagnostics
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
  set(CMAKE_COLOR_DIAGNOSTICS ON)
endif()

# Build cache configuration
option(USE_CCACHE "Use ccache for faster builds" ON)
if(USE_CCACHE)
  find_program(CCACHE_PROGRAM ccache)
  if(CCACHE_PROGRAM)
    message(STATUS "Found ccache: ${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    
    # Configure ccache for optimal performance
    execute_process(COMMAND ${CCACHE_PROGRAM} --set-config max_size=2G ERROR_QUIET)
    execute_process(COMMAND ${CCACHE_PROGRAM} --set-config compression=true ERROR_QUIET)
    execute_process(COMMAND ${CCACHE_PROGRAM} --set-config sloppiness=pch_defines,time_macros ERROR_QUIET)
  else()
    message(STATUS "ccache not found, install it for faster builds")
  endif()
endif()

# Parallel compilation
if(NOT DEFINED CMAKE_BUILD_PARALLEL_LEVEL)
  include(ProcessorCount)
  ProcessorCount(N)
  if(N GREATER 1)
    set(CMAKE_BUILD_PARALLEL_LEVEL ${N} CACHE STRING "Number of parallel build jobs")
    message(STATUS "Using ${N} parallel build jobs")
  endif()
endif()

# Final compiler options target
add_library(trinity-compile-options INTERFACE)
add_library(Trinity::CompileOptions ALIAS trinity-compile-options)

target_link_libraries(trinity-compile-options INTERFACE
  trinity-compiler-options
  trinity-debug-options
  trinity-release-options
)

# Apply warning level based on configuration
if(WITH_WARNINGS)
  if(WITH_WARNINGS STREQUAL "ALL")
    target_link_libraries(trinity-compile-options INTERFACE trinity-warning-all)
  elseif(WITH_WARNINGS STREQUAL "EXTRA")
    target_link_libraries(trinity-compile-options INTERFACE trinity-warning-extra)
  else()
    target_link_libraries(trinity-compile-options INTERFACE trinity-warning-default)
  endif()
else()
  target_link_libraries(trinity-compile-options INTERFACE trinity-warning-default)
endif()

# Function to apply Trinity compiler settings to a target
function(trinity_apply_compiler_settings target)
  target_link_libraries(${target} PRIVATE Trinity::CompileOptions)
  
  # Set properties
  set_target_properties(${target} PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
  )
  
  # Enable unity build if requested
  if(CMAKE_UNITY_BUILD)
    set_target_properties(${target} PROPERTIES
      UNITY_BUILD ON
      UNITY_BUILD_BATCH_SIZE ${CMAKE_UNITY_BUILD_BATCH_SIZE}
    )
  endif()
endfunction()