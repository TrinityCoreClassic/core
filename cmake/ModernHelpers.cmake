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

# Modern target creation with file sets
function(trinity_add_library target)
  set(options STATIC SHARED INTERFACE OBJECT)
  set(oneValueArgs ALIAS)
  set(multiValueArgs 
    SOURCES 
    PUBLIC_HEADERS 
    PRIVATE_HEADERS 
    PUBLIC_DEPS 
    PRIVATE_DEPS 
    COMPILE_FEATURES
    COMPILE_OPTIONS)
  
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
  
  # Determine library type
  if(ARG_INTERFACE)
    add_library(${target} INTERFACE)
  elseif(ARG_OBJECT)
    add_library(${target} OBJECT)
  elseif(ARG_STATIC)
    add_library(${target} STATIC)
  elseif(ARG_SHARED)
    add_library(${target} SHARED)
  else()
    add_library(${target})
  endif()
  
  # Add alias if requested
  if(ARG_ALIAS)
    add_library(${ARG_ALIAS} ALIAS ${target})
  endif()
  
  # Add sources
  if(ARG_SOURCES AND NOT ARG_INTERFACE)
    target_sources(${target} PRIVATE ${ARG_SOURCES})
  endif()
  
  # Add headers with FILE_SET (CMake 3.23+)
  if(ARG_PUBLIC_HEADERS AND NOT ARG_INTERFACE)
    target_sources(${target} 
      PUBLIC FILE_SET HEADERS 
      BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
      FILES ${ARG_PUBLIC_HEADERS})
  endif()
  
  if(ARG_PRIVATE_HEADERS AND NOT ARG_INTERFACE)
    target_sources(${target} 
      PRIVATE FILE_SET PRIVATE_HEADERS
      BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
      FILES ${ARG_PRIVATE_HEADERS})
  endif()
  
  # Add dependencies
  if(ARG_PUBLIC_DEPS)
    target_link_libraries(${target} PUBLIC ${ARG_PUBLIC_DEPS})
  endif()
  
  if(ARG_PRIVATE_DEPS AND NOT ARG_INTERFACE)
    target_link_libraries(${target} PRIVATE ${ARG_PRIVATE_DEPS})
  endif()
  
  # Add compile features
  if(ARG_COMPILE_FEATURES)
    target_compile_features(${target} PUBLIC ${ARG_COMPILE_FEATURES})
  endif()
  
  # Add compile options
  if(ARG_COMPILE_OPTIONS AND NOT ARG_INTERFACE)
    target_compile_options(${target} PRIVATE ${ARG_COMPILE_OPTIONS})
  endif()
  
  # Set default properties
  if(NOT ARG_INTERFACE)
    set_target_properties(${target} PROPERTIES
      CXX_STANDARD 23
      CXX_STANDARD_REQUIRED ON
      CXX_EXTENSIONS OFF
      POSITION_INDEPENDENT_CODE ON
      VISIBILITY_INLINES_HIDDEN ON
      FOLDER "${CMAKE_CURRENT_SOURCE_DIR}")
    
    # Enable IPO if supported
    if(CMAKE_INTERPROCEDURAL_OPTIMIZATION)
      set_target_properties(${target} PROPERTIES 
        INTERPROCEDURAL_OPTIMIZATION ON)
    endif()
  endif()
endfunction()

# Enhanced executable creation
function(trinity_add_executable target)
  set(options WIN32 MACOSX_BUNDLE)
  set(oneValueArgs FOLDER OUTPUT_NAME)
  set(multiValueArgs 
    SOURCES 
    PRIVATE_DEPS 
    COMPILE_FEATURES
    COMPILE_OPTIONS
    RUNTIME_OUTPUT_DIRECTORY)
  
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
  
  # Create executable
  if(ARG_WIN32)
    add_executable(${target} WIN32)
  elseif(ARG_MACOSX_BUNDLE)
    add_executable(${target} MACOSX_BUNDLE)
  else()
    add_executable(${target})
  endif()
  
  # Add sources
  if(ARG_SOURCES)
    target_sources(${target} PRIVATE ${ARG_SOURCES})
  endif()
  
  # Add dependencies
  if(ARG_PRIVATE_DEPS)
    target_link_libraries(${target} PRIVATE ${ARG_PRIVATE_DEPS})
  endif()
  
  # Set properties
  set_target_properties(${target} PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)
  
  if(ARG_FOLDER)
    set_target_properties(${target} PROPERTIES FOLDER ${ARG_FOLDER})
  endif()
  
  if(ARG_OUTPUT_NAME)
    set_target_properties(${target} PROPERTIES OUTPUT_NAME ${ARG_OUTPUT_NAME})
  endif()
  
  if(ARG_RUNTIME_OUTPUT_DIRECTORY)
    set_target_properties(${target} PROPERTIES 
      RUNTIME_OUTPUT_DIRECTORY ${ARG_RUNTIME_OUTPUT_DIRECTORY})
  endif()
  
  # Enable IPO if supported
  if(CMAKE_INTERPROCEDURAL_OPTIMIZATION)
    set_target_properties(${target} PROPERTIES 
      INTERPROCEDURAL_OPTIMIZATION ON)
  endif()
endfunction()

# Modern dependency management with FetchContent
function(trinity_add_dependency name)
  set(options REQUIRED QUIET)
  set(oneValueArgs 
    GIT_REPOSITORY 
    GIT_TAG 
    URL 
    URL_HASH
    SYSTEM_PACKAGE)
  set(multiValueArgs FIND_PACKAGE_ARGS CMAKE_ARGS)
  
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
  
  # First try to find system package if specified
  if(ARG_SYSTEM_PACKAGE AND NOT FORCE_BUNDLED_DEPS)
    find_package(${ARG_SYSTEM_PACKAGE} ${ARG_FIND_PACKAGE_ARGS} QUIET)
    if(${ARG_SYSTEM_PACKAGE}_FOUND)
      message(STATUS "Using system ${name}")
      return()
    endif()
  endif()
  
  # Use FetchContent for bundled dependency
  include(FetchContent)
  
  if(ARG_GIT_REPOSITORY)
    FetchContent_Declare(${name}
      GIT_REPOSITORY ${ARG_GIT_REPOSITORY}
      GIT_TAG ${ARG_GIT_TAG}
      GIT_SHALLOW TRUE
      GIT_PROGRESS TRUE
      SYSTEM
      FIND_PACKAGE_ARGS ${ARG_FIND_PACKAGE_ARGS})
  elseif(ARG_URL)
    FetchContent_Declare(${name}
      URL ${ARG_URL}
      URL_HASH ${ARG_URL_HASH}
      SYSTEM
      FIND_PACKAGE_ARGS ${ARG_FIND_PACKAGE_ARGS})
  endif()
  
  if(ARG_CMAKE_ARGS)
    FetchContent_GetProperties(${name})
    if(NOT ${name}_POPULATED)
      FetchContent_Populate(${name})
      add_subdirectory(${${name}_SOURCE_DIR} ${${name}_BINARY_DIR} ${ARG_CMAKE_ARGS})
    endif()
  else()
    FetchContent_MakeAvailable(${name})
  endif()
endfunction()

# Unity build configuration helper
function(trinity_configure_unity_build target)
  set(oneValueArgs BATCH_SIZE)
  cmake_parse_arguments(ARG "" "${oneValueArgs}" "" ${ARGN})
  
  set_target_properties(${target} PROPERTIES 
    UNITY_BUILD ON
    UNITY_BUILD_MODE BATCH)
  
  if(ARG_BATCH_SIZE)
    set_target_properties(${target} PROPERTIES 
      UNITY_BUILD_BATCH_SIZE ${ARG_BATCH_SIZE})
  else()
    set_target_properties(${target} PROPERTIES 
      UNITY_BUILD_BATCH_SIZE 16)
  endif()
endfunction()

# PCH configuration with reuse support
function(trinity_target_precompile_headers target)
  set(options REUSE)
  set(oneValueArgs REUSE_FROM)
  set(multiValueArgs PRIVATE PUBLIC INTERFACE)
  
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
  
  if(ARG_REUSE_FROM)
    target_precompile_headers(${target} REUSE_FROM ${ARG_REUSE_FROM})
  else()
    if(ARG_PRIVATE)
      target_precompile_headers(${target} PRIVATE ${ARG_PRIVATE})
    endif()
    if(ARG_PUBLIC)
      target_precompile_headers(${target} PUBLIC ${ARG_PUBLIC})
    endif()
    if(ARG_INTERFACE)
      target_precompile_headers(${target} INTERFACE ${ARG_INTERFACE})
    endif()
  endif()
endfunction()

# Install helper with component support
function(trinity_install_target target)
  set(options OPTIONAL)
  set(oneValueArgs COMPONENT DESTINATION)
  set(multiValueArgs CONFIGURATIONS)
  
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
  
  if(NOT ARG_COMPONENT)
    set(ARG_COMPONENT "Runtime")
  endif()
  
  install(TARGETS ${target}
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT ${ARG_COMPONENT}
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT ${ARG_COMPONENT}
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT "Development"
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" COMPONENT "Development"
    ${ARG_CONFIGURATIONS})
  
  # Install PDB files on Windows
  if(WIN32 AND NOT ARG_INTERFACE)
    install(FILES $<TARGET_PDB_FILE:${target}>
      DESTINATION "${CMAKE_INSTALL_BINDIR}"
      COMPONENT "Debug"
      OPTIONAL)
  endif()
endfunction()

# Helper to set up compile warnings
function(trinity_configure_warnings target)
  set(options WERROR)
  set(oneValueArgs LEVEL)
  
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "" ${ARGN})
  
  if(NOT ARG_LEVEL)
    set(ARG_LEVEL "DEFAULT")
  endif()
  
  if(MSVC)
    if(ARG_LEVEL STREQUAL "ALL")
      target_compile_options(${target} PRIVATE /W4)
    elseif(ARG_LEVEL STREQUAL "EXTRA")
      target_compile_options(${target} PRIVATE /W3)
    else()
      target_compile_options(${target} PRIVATE /W2)
    endif()
    
    if(ARG_WERROR)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    if(ARG_LEVEL STREQUAL "ALL")
      target_compile_options(${target} PRIVATE 
        -Wall -Wextra -Wpedantic
        -Wshadow -Wnon-virtual-dtor
        -Wold-style-cast -Wcast-align
        -Wunused -Woverloaded-virtual
        -Wconversion -Wsign-conversion
        -Wnull-dereference -Wdouble-promotion
        -Wformat=2)
    elseif(ARG_LEVEL STREQUAL "EXTRA")
      target_compile_options(${target} PRIVATE 
        -Wall -Wextra
        -Wshadow -Wnon-virtual-dtor
        -Wold-style-cast -Wcast-align
        -Wunused -Woverloaded-virtual)
    else()
      target_compile_options(${target} PRIVATE -Wall)
    endif()
    
    if(ARG_WERROR)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()

# Compiler cache detection and setup
function(trinity_enable_compiler_cache)
  find_program(CCACHE_PROGRAM ccache)
  find_program(SCCACHE_PROGRAM sccache)
  
  if(SCCACHE_PROGRAM)
    message(STATUS "Using sccache for compilation caching")
    set(CMAKE_C_COMPILER_LAUNCHER ${SCCACHE_PROGRAM} PARENT_SCOPE)
    set(CMAKE_CXX_COMPILER_LAUNCHER ${SCCACHE_PROGRAM} PARENT_SCOPE)
  elseif(CCACHE_PROGRAM)
    message(STATUS "Using ccache for compilation caching")
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM} PARENT_SCOPE)
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM} PARENT_SCOPE)
    
    # Configure ccache
    execute_process(COMMAND ${CCACHE_PROGRAM} -M 5G)
    execute_process(COMMAND ${CCACHE_PROGRAM} --set-config=compression=true)
    execute_process(COMMAND ${CCACHE_PROGRAM} --set-config=compression_level=6)
  else()
    message(STATUS "No compiler cache found (ccache/sccache)")
  endif()
endfunction()

# Code coverage setup
function(trinity_enable_coverage target)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE 
      --coverage
      -fprofile-arcs
      -ftest-coverage)
    target_link_options(${target} PRIVATE --coverage)
    
    # Add custom target for coverage report
    find_program(LCOV_PROGRAM lcov)
    find_program(GENHTML_PROGRAM genhtml)
    
    if(LCOV_PROGRAM AND GENHTML_PROGRAM)
      add_custom_target(coverage-${target}
        COMMAND ${LCOV_PROGRAM} --capture --directory . --output-file coverage.info
        COMMAND ${LCOV_PROGRAM} --remove coverage.info '/usr/*' --output-file coverage.info
        COMMAND ${LCOV_PROGRAM} --list coverage.info
        COMMAND ${GENHTML_PROGRAM} coverage.info --output-directory coverage
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating coverage report for ${target}")
    endif()
  endif()
endfunction()

# Sanitizer configuration
function(trinity_enable_sanitizers target)
  set(multiValueArgs SANITIZERS)
  cmake_parse_arguments(ARG "" "" "${multiValueArgs}" ${ARGN})
  
  if(NOT ARG_SANITIZERS)
    return()
  endif()
  
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(SANITIZER_FLAGS "")
    foreach(sanitizer ${ARG_SANITIZERS})
      if(sanitizer STREQUAL "ADDRESS")
        list(APPEND SANITIZER_FLAGS "-fsanitize=address" "-fno-omit-frame-pointer")
      elseif(sanitizer STREQUAL "THREAD")
        list(APPEND SANITIZER_FLAGS "-fsanitize=thread")
      elseif(sanitizer STREQUAL "MEMORY")
        list(APPEND SANITIZER_FLAGS "-fsanitize=memory")
      elseif(sanitizer STREQUAL "UNDEFINED")
        list(APPEND SANITIZER_FLAGS "-fsanitize=undefined")
      endif()
    endforeach()
    
    target_compile_options(${target} PRIVATE ${SANITIZER_FLAGS})
    target_link_options(${target} PRIVATE ${SANITIZER_FLAGS})
  endif()
endfunction()