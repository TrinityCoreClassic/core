# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

cmake_minimum_required(VERSION 3.24)

# Add module paths BEFORE project() to override FindOpenSSL.cmake early
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/macros")

# add this options before PROJECT keyword
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

# Set projectname (must be done AFTER setting configurationtypes)
project(TrinityCore)

if(POLICY CMP0144)
  cmake_policy(SET CMP0144 NEW) # find_package() uses upper-case <PACKAGENAME>_ROOT variables
endif()

if(POLICY CMP0153)
  cmake_policy(SET CMP0153 NEW) # The exec_program() command should not be called
endif()

# Set RPATH-handling (CMake parameters)
set(CMAKE_SKIP_BUILD_RPATH 0)
set(CMAKE_BUILD_WITH_INSTALL_RPATH 0)
list(APPEND CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH 1)

# build in Release-mode by default if not explicitly set
if(CMAKE_GENERATOR STREQUAL "Ninja Multi-Config")
  set(CMAKE_DEFAULT_BUILD_TYPE "RelWithDebInfo" CACHE INTERNAL "")
endif()
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif()

include(CheckCXXSourceRuns)
include(CheckIncludeFiles)
include(ConfigureScripts)

# set default buildoptions and print them
include(cmake/options.cmake)

# turn off PCH totally if enabled (hidden setting, mainly for devs)
if(NOPCH)
  set(USE_COREPCH 0)
  set(USE_SCRIPTPCH 0)
endif()

include(ConfigureBaseTargets)
include(CheckPlatform)

include(GroupSources)
include(AutoCollect)

find_package(PCHSupport)

if(NOT WITHOUT_GIT)
  find_package(Git 1.7)
endif()

# find mysql client binary (needed by genrev)
find_package(MySQL OPTIONAL_COMPONENTS binary)

# Find revision ID and hash of the sourcetree
include(cmake/genrev.cmake)

# print out the results before continuing
include(cmake/showoptions.cmake)

# --- Begin final OpenSSL detection ---
if (WIN32)
    set(_OPENSSL_HINTS "")

    # 1. Add OPENSSL_ROOT_DIR env var if it's set
    if(DEFINED ENV{OPENSSL_ROOT_DIR})
        list(APPEND _OPENSSL_HINTS "$ENV{OPENSSL_ROOT_DIR}")
    endif()

    # 2. Add standard install locations
    if (DEFINED ENV{ProgramFiles})
        list(APPEND _OPENSSL_HINTS
            "$ENV{ProgramFiles}/OpenSSL-Win64"
            "$ENV{ProgramFiles}/OpenSSL-Win32")
    endif()

    # 3. Try detection
    set(_OPENSSL_FOUND FALSE)
    foreach(_path IN LISTS _OPENSSL_HINTS)
        if (EXISTS "${_path}/lib/VC/x64/MD/libssl.lib")
            message(STATUS "Auto-detected OpenSSL at: ${_path}")
            set(OPENSSL_ROOT_DIR "${_path}" CACHE PATH "OpenSSL root directory" FORCE)
            set(OPENSSL_INCLUDE_DIR "${_path}/include" CACHE PATH "OpenSSL include directory" FORCE)
            set(OPENSSL_SSL_LIBRARY "${_path}/lib/VC/x64/MD/libssl.lib" CACHE FILEPATH "SSL library" FORCE)
            set(OPENSSL_CRYPTO_LIBRARY "${_path}/lib/VC/x64/MD/libcrypto.lib" CACHE FILEPATH "Crypto library" FORCE)
            set(_OPENSSL_FOUND TRUE)
            break()
        endif()
    endforeach()

    # 4. Fail if not found
    if (NOT _OPENSSL_FOUND)
        message(FATAL_ERROR
  "Could not find OpenSSL!\n"
  "Install OpenSSL, set the OPENSSL_ROOT_DIR environment variable or install to a standard location: C:/Program Files/OpenSSL-Win64\.n"
  "Then:"
  "\n"
  "  - Delete CMake cache.\n"
  "  - Clear build output directory.\n"
  "  - Restart CMake and configure again."
)
    endif()
endif()
# --- End final OpenSSL detection ---

# add dependencies
add_subdirectory(dep)

# add core sources
add_subdirectory(src)

include(CTest)
if(BUILD_TESTING)
  list(APPEND CMAKE_MODULE_PATH
    "${Catch2_SOURCE_DIR}/contrib")
  include(Catch)

  add_subdirectory(tests)

  # Catch cmakefile messes with our settings we explicitly leave up to the user
  # restore user preference
  if (NOT WITH_SOURCE_TREE STREQUAL "hierarchical-folders")
    set_property(GLOBAL PROPERTY USE_FOLDERS OFF)
  endif()
endif()

