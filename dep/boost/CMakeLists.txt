# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

set(BOOST_SEARCH_HINTS)
if(WIN32)
  if(DEFINED ENV{BOOST_ROOT})
    set(BOOST_ROOT $ENV{BOOST_ROOT})
  endif()
  if(DEFINED BOOST_ROOT AND MSVC)
    # insert a dot (.) character before last digit of MSVC_TOOLSET_VERSION
    # turn 143 into 14.3
    string(LENGTH "${MSVC_TOOLSET_VERSION}" _BOOST_MSVC_TOOLSET_VERSION_LENGTH)
    math(EXPR _BOOST_MSVC_TOOLSET_VERSION_LENGTH "${_BOOST_MSVC_TOOLSET_VERSION_LENGTH} - 1" OUTPUT_FORMAT DECIMAL)
    string(SUBSTRING "${MSVC_TOOLSET_VERSION}" 0 ${_BOOST_MSVC_TOOLSET_VERSION_LENGTH} _BOOST_MSVC_TOOLSET_VERSION_MAJOR)
    string(SUBSTRING "${MSVC_TOOLSET_VERSION}" ${_BOOST_MSVC_TOOLSET_VERSION_LENGTH} -1 _BOOST_MSVC_TOOLSET_VERSION_MINOR)

    file(TO_CMAKE_PATH "${BOOST_ROOT}" BOOST_ROOT)
    set(BOOST_SEARCH_HINTS "${BOOST_ROOT}/lib${PLATFORM}-msvc-${_BOOST_MSVC_TOOLSET_VERSION_MAJOR}.${_BOOST_MSVC_TOOLSET_VERSION_MINOR}/cmake")

    unset(_BOOST_MSVC_TOOLSET_VERSION_LENGTH)
    unset(_BOOST_MSVC_TOOLSET_VERSION_MAJOR)
    unset(_BOOST_MSVC_TOOLSET_VERSION_MINOR)
  endif()

  set(Boost_USE_STATIC_LIBS        ON)
  set(Boost_USE_MULTITHREADED      ON)
  set(Boost_USE_STATIC_RUNTIME     OFF)
endif()

if (WIN32)
  # On windows the requirements are higher according to the wiki.
  set(BOOST_REQUIRED_VERSION_MIN 1.78) 
  set(BOOST_REQUIRED_VERSION_MAX 1.84)  
else()
  set(BOOST_REQUIRED_VERSION 1.74)
endif()

find_package(Boost ${BOOST_REQUIRED_VERSION}
  COMPONENTS 
    system
    filesystem
    program_options
    regex
    locale
  CONFIG
  HINTS
    ${BOOST_SEARCH_HINTS}
)

if (NOT Boost_FOUND)
message(FATAL_ERROR
" Could not find Boost ${BOOST_REQUIRED_VERSION_MIN} – ${BOOST_REQUIRED_VERSION_MAX}!\n"
" Install Boost and set BOOST_ROOT environment variable."
"\n"
"Then:\n"
"  - Delete CMake cache.\n"
"  - Clear your build output directory.\n"
"  - Restart CMake and configure again."
)
endif()

add_library(boost INTERFACE)

target_link_libraries(boost
  INTERFACE
    ${Boost_LIBRARIES})

target_include_directories(boost
  INTERFACE
    ${Boost_INCLUDE_DIRS})

target_compile_definitions(boost
  INTERFACE
    BOOST_ALL_NO_LIB
    BOOST_CONFIG_SUPPRESS_OUTDATED_MESSAGE
    BOOST_ASIO_NO_DEPRECATED
    BOOST_BIND_NO_PLACEHOLDERS
    BOOST_SYSTEM_USE_UTF8)

if (WITH_BOOST_STACKTRACE AND NOT WIN32)
  message(STATUS "libbacktrace will be linked")

  include(CheckIncludeFile)

  if (BOOST_STACKTRACE_BACKTRACE_INCLUDE_FILE)
    check_include_file("${BOOST_STACKTRACE_BACKTRACE_INCLUDE_FILE}" HAS_BACKTRACE)
    target_compile_definitions(boost
      INTERFACE
        BOOST_STACKTRACE_BACKTRACE_INCLUDE_FILE=${BOOST_STACKTRACE_BACKTRACE_INCLUDE_FILE})
  else()
    check_include_file("backtrace.h" HAS_BACKTRACE)
  endif()

  if (NOT HAS_BACKTRACE)
    message(FATAL_ERROR "Required header 'backtrace.h' not found. If building with a compiler other than GCC, please specify the full path in the CMake option BOOST_STACKTRACE_BACKTRACE_INCLUDE_FILE.")
  endif()

  target_compile_definitions(boost
    INTERFACE
      BOOST_STACKTRACE_USE_BACKTRACE)

  target_link_libraries(boost
    INTERFACE
      backtrace)
endif()
