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

# Option to prefer system libraries over bundled ones
option(PREFER_SYSTEM_LIBS "Prefer system libraries over bundled ones" OFF)
option(FORCE_BUNDLED_LIBS "Force use of bundled libraries" OFF)

# Include FetchContent
include(FetchContent)

# Set FetchContent defaults
set(FETCHCONTENT_QUIET OFF)
set(FETCHCONTENT_TRY_FIND_PACKAGE_MODE ALWAYS)


# Dependency versions
set(TRINITY_BOOST_MIN_VERSION "1.74")
set(TRINITY_OPENSSL_MIN_VERSION "3.0")
set(TRINITY_MYSQL_MIN_VERSION "8.0")
set(TRINITY_FMT_VERSION "10.1.0")
set(TRINITY_CATCH2_VERSION "3.4.0")
set(TRINITY_PROTOBUF_VERSION "2.6.1")

# Boost configuration
function(trinity_find_boost)
  # Handle CMake 3.30+ policy for FindBoost removal
  if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
  endif()
  
  set(Boost_NO_WARN_NEW_VERSIONS ON)
  
  if(WIN32)
    set(Boost_USE_STATIC_LIBS ON)
    set(Boost_USE_MULTITHREADED ON)
    set(Boost_USE_STATIC_RUNTIME OFF)
  endif()
  
  find_package(Boost ${TRINITY_BOOST_MIN_VERSION} REQUIRED
    COMPONENTS
      system
      filesystem
      program_options
      locale
      thread
      regex
      chrono
      atomic
  )
  
  # Create modern target
  if(NOT TARGET Trinity::Boost)
    add_library(Trinity::Boost INTERFACE IMPORTED)
    
    # Handle Boost libraries properly - filter out link keywords
    set(_boost_libs_filtered)
    set(_skip_next FALSE)
    foreach(_lib ${Boost_LIBRARIES})
      if(_skip_next)
        set(_skip_next FALSE)
      elseif(_lib STREQUAL "optimized" OR _lib STREQUAL "debug")
        set(_skip_next TRUE)
      else()
        list(APPEND _boost_libs_filtered ${_lib})
      endif()
    endforeach()
    
    set_target_properties(Trinity::Boost PROPERTIES
      INTERFACE_LINK_LIBRARIES "${_boost_libs_filtered}"
      INTERFACE_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}"
      INTERFACE_COMPILE_DEFINITIONS "BOOST_ALL_NO_LIB;BOOST_CONFIG_SUPPRESS_OUTDATED_MESSAGE"
    )
  endif()
endfunction()

# OpenSSL configuration
function(trinity_find_openssl)
  if(WIN32)
    # Windows-specific OpenSSL detection
    set(_OPENSSL_HINTS "")
    
    # 1. Add OPENSSL_ROOT_DIR env var if it's set
    if(DEFINED ENV{OPENSSL_ROOT_DIR})
      list(APPEND _OPENSSL_HINTS "$ENV{OPENSSL_ROOT_DIR}")
    endif()
    
    # 2. Add standard install locations
    if(DEFINED ENV{ProgramFiles})
      list(APPEND _OPENSSL_HINTS
        "$ENV{ProgramFiles}/OpenSSL-Win64"
        "$ENV{ProgramFiles}/OpenSSL-Win32"
        "$ENV{ProgramFiles}/OpenSSL"
      )
    endif()
    
    # Handle ProgramFiles(x86) environment variable with parentheses
    set(_progfiles_x86 "ProgramFiles(x86)")
    if(DEFINED ENV{${_progfiles_x86}})
      list(APPEND _OPENSSL_HINTS
        "$ENV{${_progfiles_x86}}/OpenSSL-Win32"
        "$ENV{${_progfiles_x86}}/OpenSSL"
      )
    endif()
    
    # 3. Try auto-detection
    set(_OPENSSL_FOUND FALSE)
    foreach(_path IN LISTS _OPENSSL_HINTS)
      if(EXISTS "${_path}/lib/VC/x64/MD/libssl.lib")
        message(STATUS "Auto-detected OpenSSL at: ${_path}")
        set(OPENSSL_ROOT_DIR "${_path}" CACHE PATH "OpenSSL root directory" FORCE)
        set(OPENSSL_INCLUDE_DIR "${_path}/include" CACHE PATH "OpenSSL include directory" FORCE)
        set(OPENSSL_SSL_LIBRARY "${_path}/lib/VC/x64/MD/libssl.lib" CACHE FILEPATH "SSL library" FORCE)
        set(OPENSSL_CRYPTO_LIBRARY "${_path}/lib/VC/x64/MD/libcrypto.lib" CACHE FILEPATH "Crypto library" FORCE)
        set(_OPENSSL_FOUND TRUE)
        break()
      elseif(EXISTS "${_path}/lib/libssl.lib")
        # Try alternate library layout
        message(STATUS "Auto-detected OpenSSL at: ${_path}")
        set(OPENSSL_ROOT_DIR "${_path}" CACHE PATH "OpenSSL root directory" FORCE)
        set(OPENSSL_INCLUDE_DIR "${_path}/include" CACHE PATH "OpenSSL include directory" FORCE)
        set(OPENSSL_SSL_LIBRARY "${_path}/lib/libssl.lib" CACHE FILEPATH "SSL library" FORCE)
        set(OPENSSL_CRYPTO_LIBRARY "${_path}/lib/libcrypto.lib" CACHE FILEPATH "Crypto library" FORCE)
        set(_OPENSSL_FOUND TRUE)
        break()
      endif()
    endforeach()
    
    if(NOT _OPENSSL_FOUND AND NOT OPENSSL_ROOT_DIR)
      message(WARNING
        "Could not auto-detect OpenSSL on Windows!\n"
        "Please either:\n"
        "  1. Install OpenSSL to a standard location (e.g., C:/Program Files/OpenSSL-Win64)\n"
        "  2. Set the OPENSSL_ROOT_DIR environment variable to your OpenSSL installation\n"
        "  3. Set OPENSSL_ROOT_DIR in CMake GUI or command line\n"
        "After setting the path, delete CMakeCache.txt and reconfigure."
      )
    endif()
  elseif(APPLE)
    # Help find Homebrew's OpenSSL on macOS
    execute_process(
      COMMAND brew --prefix openssl@3
      OUTPUT_VARIABLE OPENSSL_ROOT_DIR
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )
    if(NOT OPENSSL_ROOT_DIR OR NOT EXISTS "${OPENSSL_ROOT_DIR}")
      # Try OpenSSL 1.1 if 3.x not found
      execute_process(
        COMMAND brew --prefix openssl@1.1
        OUTPUT_VARIABLE OPENSSL_ROOT_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
      )
    endif()
  endif()
  
  # Now try to find OpenSSL with the hints we've set up
  find_package(OpenSSL ${TRINITY_OPENSSL_MIN_VERSION} REQUIRED)
  
  # Create unified target
  if(NOT TARGET Trinity::OpenSSL)
    add_library(Trinity::OpenSSL INTERFACE IMPORTED)
    set_target_properties(Trinity::OpenSSL PROPERTIES
      INTERFACE_LINK_LIBRARIES "OpenSSL::SSL;OpenSSL::Crypto"
    )
  endif()
endfunction()

# MySQL configuration
function(trinity_find_mysql)
  # First try to find MySQL
  find_package(MySQL QUIET COMPONENTS lib)
  
  if(NOT MySQL_FOUND)
    # Try MariaDB
    find_path(MYSQL_INCLUDE_DIR
      NAMES mysql.h
      PATHS
        /usr/include/mysql
        /usr/include/mariadb
        /usr/local/include/mysql
        /usr/local/include/mariadb
    )
    
    find_library(MYSQL_LIBRARY
      NAMES mysqlclient mariadbclient
      PATHS
        /usr/lib
        /usr/lib/mysql
        /usr/lib/mariadb
        /usr/local/lib
        /usr/local/lib/mysql
        /usr/local/lib/mariadb
    )
    
    if(MYSQL_INCLUDE_DIR AND MYSQL_LIBRARY)
      set(MySQL_FOUND TRUE)
    endif()
  endif()
  
  if(NOT MySQL_FOUND)
    message(FATAL_ERROR "MySQL/MariaDB development libraries not found")
  endif()
  
  # Create modern target
  if(NOT TARGET Trinity::MySQL)
    add_library(Trinity::MySQL INTERFACE IMPORTED)
    set_target_properties(Trinity::MySQL PROPERTIES
      INTERFACE_LINK_LIBRARIES "${MYSQL_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${MYSQL_INCLUDE_DIR}"
    )
  endif()
endfunction()

# fmt library configuration
function(trinity_find_fmt)
  if(NOT FORCE_BUNDLED_LIBS)
    find_package(fmt ${TRINITY_FMT_VERSION} QUIET)
  endif()
  
  if(NOT fmt_FOUND)
    message(STATUS "Using bundled fmt library")
    FetchContent_Declare(fmt
      GIT_REPOSITORY https://github.com/fmtlib/fmt.git
      GIT_TAG ${TRINITY_FMT_VERSION}
      GIT_SHALLOW TRUE
      SYSTEM
    )
    FetchContent_MakeAvailable(fmt)
  endif()
  
  # Create alias - check if we're using the bundled version from dep/fmt
  if(NOT TARGET Trinity::fmt)
    if(TARGET fmt::fmt)
      # Check if fmt::fmt is an alias target
      get_target_property(_aliased_target fmt::fmt ALIASED_TARGET)
      if(_aliased_target)
        # fmt::fmt is an alias, create interface target instead
        add_library(Trinity::fmt INTERFACE IMPORTED)
        set_target_properties(Trinity::fmt PROPERTIES
          INTERFACE_LINK_LIBRARIES fmt::fmt
        )
      else()
        # fmt::fmt is not an alias, safe to create alias
        add_library(Trinity::fmt ALIAS fmt::fmt)
      endif()
    elseif(TARGET fmt)
      # If 'fmt' target exists, it might be from our bundled version
      # Create an interface target instead of an alias to avoid conflicts
      add_library(Trinity::fmt INTERFACE IMPORTED)
      set_target_properties(Trinity::fmt PROPERTIES
        INTERFACE_LINK_LIBRARIES fmt
      )
    endif()
  endif()
endfunction()

# Protobuf configuration
function(trinity_find_protobuf)
  # Initialize to build from source by default
  set(BUILD_PROTOBUF_FROM_SOURCE TRUE)
  
  # First, check if system protobuf headers exist to avoid CMake errors
  set(PROTOBUF_HEADER_PATH "/usr/include/google/protobuf/stubs/common.h")
  
  if(EXISTS ${PROTOBUF_HEADER_PATH})
    # Headers exist, try to find system protobuf
    find_package(Protobuf QUIET)
    
    if(Protobuf_FOUND)
      # Check if the found version is exactly 2.6.1
      if(Protobuf_VERSION)
        if(Protobuf_VERSION VERSION_EQUAL "2.6.1")
          message(STATUS "Found system protobuf version 2.6.1 - using it")
          set(BUILD_PROTOBUF_FROM_SOURCE FALSE)
        else()
          message(STATUS "Found system protobuf version ${Protobuf_VERSION}, but need 2.6.1 - will build from source")
        endif()
      else()
        # Older FindProtobuf modules might not set Protobuf_VERSION
        message(STATUS "Found system protobuf but couldn't determine version - will build from source")
      endif()
    else()
      message(STATUS "System protobuf not found - will build version 2.6.1 from source")
    endif()
  else()
    message(STATUS "System protobuf headers not found - will build version 2.6.1 from source")
  endif()
  
  if(BUILD_PROTOBUF_FROM_SOURCE)
    message(STATUS "Building protobuf v2.6.1 from source using CMake (configure time)")
    
    include(FetchContent)
    
    # Build protobuf v2.6.1 at configure time
    set(PROTOBUF_PREFIX ${CMAKE_BINARY_DIR}/protobuf-2.6.1)
    set(PROTOBUF_SOURCE_DIR ${PROTOBUF_PREFIX}/src)
    
    # Download protobuf source if not already done
    FetchContent_Declare(
      protobuf_source
      GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
      GIT_TAG        v2.6.1
      GIT_SHALLOW    TRUE
      SOURCE_DIR     ${PROTOBUF_SOURCE_DIR}
    )
    
    FetchContent_GetProperties(protobuf_source)
    if(NOT protobuf_source_POPULATED)
      message(STATUS "Downloading protobuf v2.6.1 source...")
      FetchContent_Populate(protobuf_source)
    endif()
    
    # Set variables for the built protobuf
    set(PROTOBUF_FOUND TRUE)
    set(PROTOBUF_INCLUDE_DIRS ${PROTOBUF_PREFIX}/install/include)
    set(PROTOBUF_LIBRARIES ${PROTOBUF_PREFIX}/install/lib/libprotobuf.a)
    set(PROTOBUF_PROTOC_EXECUTABLE ${PROTOBUF_PREFIX}/install/bin/protoc)
    
    # Check if we need to build protobuf
    if(NOT EXISTS ${PROTOBUF_LIBRARIES})
      message(STATUS "Building protobuf v2.6.1 at configure time (this may take a while)...")
      
      # Create build directories
      file(MAKE_DIRECTORY ${PROTOBUF_PREFIX}/build)
      file(MAKE_DIRECTORY ${PROTOBUF_PREFIX}/install)
      file(MAKE_DIRECTORY ${PROTOBUF_INCLUDE_DIRS})
      
      # Copy CMake files to source
      execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy_directory 
          ${CMAKE_SOURCE_DIR}/cmake/protobuf-2.6.1-cmake/
          ${PROTOBUF_SOURCE_DIR}/
        RESULT_VARIABLE COPY_RESULT
      )
      
      if(NOT COPY_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to copy CMake files to protobuf source")
      endif()
      
      # Apply va_copy patch (ignore errors if already applied)
      execute_process(
        COMMAND patch -p1 -N
        INPUT_FILE ${CMAKE_SOURCE_DIR}/cmake/protobuf-2.6.1-cmake/patches/add-va-copy-fix.patch
        WORKING_DIRECTORY ${PROTOBUF_SOURCE_DIR}
        OUTPUT_QUIET
        ERROR_QUIET
      )
      
      # Configure protobuf build
      set(PROTOBUF_CMAKE_ARGS
          -DCMAKE_INSTALL_PREFIX=${PROTOBUF_PREFIX}/install
          -DCMAKE_BUILD_TYPE=Release
          -DBUILD_SHARED_LIBS=OFF
          -DBUILD_TESTING=OFF
          -DCMAKE_POSITION_INDEPENDENT_CODE=ON
      )
      
      if(CMAKE_CXX_COMPILER)
        list(APPEND PROTOBUF_CMAKE_ARGS "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}")
      endif()
      if(CMAKE_C_COMPILER)
        list(APPEND PROTOBUF_CMAKE_ARGS "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}")
      endif()
      
      execute_process(
        COMMAND ${CMAKE_COMMAND} ${PROTOBUF_CMAKE_ARGS} ${PROTOBUF_SOURCE_DIR}
        WORKING_DIRECTORY ${PROTOBUF_PREFIX}/build
        OUTPUT_VARIABLE CONFIGURE_OUTPUT
        ERROR_VARIABLE CONFIGURE_ERROR
        RESULT_VARIABLE CONFIGURE_RESULT
      )
      
      if(NOT CONFIGURE_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to configure protobuf:\n${CONFIGURE_ERROR}")
      endif()
      
      # Build protobuf
      include(ProcessorCount)
      ProcessorCount(N)
      if(NOT N EQUAL 0)
        set(MAKE_JOBS --parallel ${N})
      endif()
      
      execute_process(
        COMMAND ${CMAKE_COMMAND} --build . ${MAKE_JOBS}
        WORKING_DIRECTORY ${PROTOBUF_PREFIX}/build
        OUTPUT_VARIABLE BUILD_OUTPUT
        ERROR_VARIABLE BUILD_ERROR
        RESULT_VARIABLE BUILD_RESULT
      )
      
      if(NOT BUILD_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to build protobuf:\n${BUILD_ERROR}")
      endif()
      
      # Install protobuf
      execute_process(
        COMMAND ${CMAKE_COMMAND} --build . --target install
        WORKING_DIRECTORY ${PROTOBUF_PREFIX}/build
        OUTPUT_VARIABLE INSTALL_OUTPUT
        ERROR_VARIABLE INSTALL_ERROR
        RESULT_VARIABLE INSTALL_RESULT
      )
      
      if(NOT INSTALL_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to install protobuf:\n${INSTALL_ERROR}")
      endif()
      
      message(STATUS "Successfully built and installed protobuf v2.6.1")
    else()
      message(STATUS "Using cached protobuf v2.6.1 build")
    endif()
    
    # Create imported target for cleaner linking
    add_library(protobuf::libprotobuf STATIC IMPORTED GLOBAL)
    set_target_properties(protobuf::libprotobuf PROPERTIES
        IMPORTED_LOCATION ${PROTOBUF_LIBRARIES}
        INTERFACE_INCLUDE_DIRECTORIES ${PROTOBUF_INCLUDE_DIRS}
    )
    
    # Set in parent scope for proto CMakeLists
    set(PROTOBUF_INCLUDE_DIRS ${PROTOBUF_INCLUDE_DIRS} PARENT_SCOPE)
    set(PROTOBUF_LIBRARIES ${PROTOBUF_LIBRARIES} PARENT_SCOPE)
    set(PROTOBUF_PROTOC_EXECUTABLE ${PROTOBUF_PROTOC_EXECUTABLE} PARENT_SCOPE)
    set(PROTOBUF_FOUND ${PROTOBUF_FOUND} PARENT_SCOPE)
  endif()
  
  # Create unified target
  if(NOT TARGET Trinity::Protobuf)
    add_library(Trinity::Protobuf INTERFACE IMPORTED)
    if(BUILD_PROTOBUF_FROM_SOURCE)
      set_target_properties(Trinity::Protobuf PROPERTIES
        INTERFACE_LINK_LIBRARIES protobuf::libprotobuf
      )
    else()
      set_target_properties(Trinity::Protobuf PROPERTIES
        INTERFACE_LINK_LIBRARIES "${PROTOBUF_LIBRARIES}"
        INTERFACE_INCLUDE_DIRECTORIES "${PROTOBUF_INCLUDE_DIRS}"
      )
    endif()
  endif()
endfunction()

# Catch2 for testing
function(trinity_find_catch2)
  if(NOT BUILD_TESTING)
    return()
  endif()
  
  FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v${TRINITY_CATCH2_VERSION}
    GIT_SHALLOW TRUE
    SYSTEM
    FIND_PACKAGE_ARGS ${TRINITY_CATCH2_VERSION}
  )
  FetchContent_MakeAvailable(Catch2)
  
  # Include Catch2 extras
  list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
  include(Catch)
endfunction()

# Thread library
function(trinity_find_threads)
  set(CMAKE_THREAD_PREFER_PTHREAD ON)
  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads REQUIRED)
  
  if(NOT TARGET Trinity::Threads)
    add_library(Trinity::Threads ALIAS Threads::Threads)
  endif()
endfunction()

# ZLIB configuration
function(trinity_find_zlib)
  if(NOT FORCE_BUNDLED_LIBS)
    find_package(ZLIB QUIET)
  endif()
  
  if(NOT ZLIB_FOUND)
    message(STATUS "Using bundled zlib")
    set(ZLIB_USE_STATIC_LIBS ON)
    
    FetchContent_Declare(zlib
      URL https://github.com/madler/zlib/releases/download/v1.3/zlib-1.3.tar.gz
      URL_HASH SHA256=ff0ba4c292013dbc27530b3a81e1f9a813cd39de01ca5e0f8bf355702efa593e
      SYSTEM
    )
    FetchContent_MakeAvailable(zlib)
    
    # Create ZLIB target
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
  endif()
  
  if(NOT TARGET Trinity::ZLIB)
    add_library(Trinity::ZLIB ALIAS ZLIB::ZLIB)
  endif()
endfunction()

# Readline configuration
function(trinity_find_readline)
  if(WIN32)
    # Windows doesn't typically have readline
    return()
  endif()
  
  find_package(Readline QUIET)
  
  if(Readline_FOUND AND NOT TARGET Trinity::Readline)
    add_library(Trinity::Readline INTERFACE IMPORTED)
    set_target_properties(Trinity::Readline PROPERTIES
      INTERFACE_LINK_LIBRARIES "${READLINE_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${READLINE_INCLUDE_DIR}"
    )
  endif()
endfunction()

# Main dependency resolution function
function(trinity_resolve_dependencies)
  message(STATUS "Resolving TrinityCore dependencies...")
  
  # Enable compiler cache if available
  if(NOT DEFINED CMAKE_CXX_COMPILER_LAUNCHER)
    include(ModernHelpers)
    trinity_enable_compiler_cache()
  endif()
  
  # Core dependencies
  trinity_find_threads()
  trinity_find_boost()
  trinity_find_openssl()
  trinity_find_zlib()
  
  # Server dependencies
  if(SERVERS)
    trinity_find_mysql()
    trinity_find_readline()
    trinity_find_protobuf()
  endif()
  
  # Development dependencies
  trinity_find_fmt()
  
  # Testing dependencies
  if(BUILD_TESTING)
    trinity_find_catch2()
  endif()
  
  message(STATUS "All dependencies resolved successfully")
endfunction()


# Export dependency information for installation
function(trinity_export_dependencies)
  # Generate dependency information file
  configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/TrinityCoreDependencies.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/TrinityCoreDependencies.cmake
    @ONLY
  )
  
  # Install the dependency file
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/TrinityCoreDependencies.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TrinityCore
    COMPONENT Development
  )
endfunction()