# CMake build file for Google Protocol Buffers 2.6.1
# This replaces the autotools-based build system
cmake_minimum_required(VERSION 3.10)
project(protobuf VERSION 2.6.1 LANGUAGES C CXX)

# Options
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(BUILD_TESTING "Build tests" OFF)
option(BUILD_EXAMPLES "Build examples" OFF)

# C++ Standard
set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# C Standard for va_copy support
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Position independent code
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Generate config.h from template
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_SOURCE_DIR}/src/config.h @ONLY)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Find zlib (optional compression support)
find_package(ZLIB)
if(ZLIB_FOUND)
  set(HAVE_ZLIB 1)
  include_directories(${ZLIB_INCLUDE_DIRS})
  message(STATUS "Found ZLIB, protobuf will be built with compression support")
else()
  message(STATUS "ZLIB not found, protobuf will be built without compression support")
endif()

# Platform-specific definitions
if(WIN32)
  add_definitions(-DWIN32 -D_WINDOWS -D_CRT_SECURE_NO_WARNINGS)
  if(MSVC)
    # Suppress some Visual Studio warnings
    add_compile_options(/wd4244 /wd4267 /wd4996 /wd4800)
    # Use static runtime
    foreach(flag_var
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
      if(${flag_var} MATCHES "/MD")
        string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
      endif()
    endforeach()
  endif()
else()
  # Unix-like systems
  add_definitions(-DHAVE_PTHREAD -D_GNU_SOURCE)
  set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
  find_package(Threads REQUIRED)
endif()

# Add subdirectory with actual library definitions
add_subdirectory(src)

# Installation rules
install(DIRECTORY src/google/protobuf/
  DESTINATION include/google/protobuf
  FILES_MATCHING
  PATTERN "*.h"
  PATTERN "*.proto"
  PATTERN "*.inc"
  PATTERN "*test*" EXCLUDE
  PATTERN "*unittest*" EXCLUDE
  PATTERN "testdata" EXCLUDE)

# Generate protobuf-config.cmake for find_package support
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/protobuf-config-version.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/protobuf-config-version.cmake"
  DESTINATION lib/cmake/protobuf
)