# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

cmake_minimum_required(VERSION 3.28)

# Add module paths BEFORE project() to override FindOpenSSL.cmake early
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/macros")

# add this options before PROJECT keyword
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

# Enable better error messages for debugging
set(CMAKE_MESSAGE_CONTEXT_SHOW ON CACHE BOOL "Show message context")
list(APPEND CMAKE_MESSAGE_CONTEXT "TrinityCore")

# Set up dependency provider (must be done before project())
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
  set(CMAKE_PROJECT_TOP_LEVEL_INCLUDES "${CMAKE_SOURCE_DIR}/cmake/DependencyProvider.cmake")
endif()

# Set projectname with version information
project(TrinityCore
  VERSION 1.14.0
  DESCRIPTION "TrinityCore Classic - MMORPG Framework"
  HOMEPAGE_URL "https://www.trinitycore.org"
  LANGUAGES CXX C)

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

# set macro-directory
list(APPEND CMAKE_MODULE_PATH
  "${CMAKE_SOURCE_DIR}/cmake/macros"
  "${CMAKE_SOURCE_DIR}/cmake")

# build in Release-mode by default if not explicitly set
if(CMAKE_GENERATOR STREQUAL "Ninja Multi-Config")
  set(CMAKE_DEFAULT_BUILD_TYPE "RelWithDebInfo" CACHE INTERNAL "")
endif()
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif()

# Enable compile commands for better IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable color diagnostics
set(CMAKE_COLOR_DIAGNOSTICS ON)

# Optimize build dependencies
set(CMAKE_OPTIMIZE_DEPENDENCIES ON)

include(CheckCXXSourceRuns)
include(CheckIncludeFiles)
include(ConfigureScripts)

# set default buildoptions and print them
include(cmake/options.cmake)

# Include modern CMake helpers
include(ModernHelpers)
include(CompilerSettings)
include(Dependencies)

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
find_package(MySQL REQUIRED COMPONENTS lib OPTIONAL_COMPONENTS binary)

# Find revision ID and hash of the sourcetree
include(cmake/genrev.cmake)

# print out the results before continuing
include(cmake/showoptions.cmake)

# Resolve all dependencies
trinity_resolve_dependencies()

# add dependencies
add_subdirectory(dep)

# add core sources
add_subdirectory(src)

include(CTest)
if(BUILD_TESTING)
  # The test directory will handle Catch2 setup
  if(EXISTS "${CMAKE_SOURCE_DIR}/tests")
    add_subdirectory(tests)
  endif()

  # Catch cmakefile messes with our settings we explicitly leave up to the user
  # restore user preference
  if (NOT WITH_SOURCE_TREE STREQUAL "hierarchical-folders")
    set_property(GLOBAL PROPERTY USE_FOLDERS OFF)
  endif()
endif()

# Include packaging configuration
include(Packaging)
