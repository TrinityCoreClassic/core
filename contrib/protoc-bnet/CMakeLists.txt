cmake_minimum_required(VERSION 3.28)
project(protoc_bnet)

set(CMAKE_CXX_EXTENSIONS OFF)

# Use the centralized protobuf from Dependencies.cmake
if(NOT TARGET Trinity::Protobuf)
  message(FATAL_ERROR "Trinity::Protobuf target not found. This tool must be built as part of TrinityCore.")
endif()

# Include the bundled protobuf compiler headers
# These are internal headers not normally installed by protobuf
file(GLOB_RECURSE SOURCE_PROTOBUF_CPP google/protobuf/*.cc)

set(SOURCE_FILES
  main.cpp
  BnetCodeGenerator.cpp
  BnetFileGenerator.cpp
  BnetServiceGenerator.cpp
  method_options.pb.cc
  service_options.pb.cc
  ${SOURCE_PROTOBUF_CPP})

add_executable(protoc-gen-bnet ${SOURCE_FILES})

# Use C++11 (minimum needed for protoc-gen-bnet tool features)
set_target_properties(protoc-gen-bnet PROPERTIES
  CXX_STANDARD 11
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF)

# Add compatibility definitions and disable warnings for old protobuf code
target_compile_definitions(protoc-gen-bnet
  PRIVATE
    _SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS)

# Disable warnings that are problematic with old C++98 code
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(protoc-gen-bnet
    PRIVATE
      -Wno-deprecated
      -Wno-deprecated-declarations
      -Wno-unused-parameter
      -Wno-sign-compare)
endif()

# Include current directory for local headers
target_include_directories(protoc-gen-bnet
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})

# Link against the centralized protobuf
# Note: protobuf 2.6.1 doesn't provide separate libprotoc target
# so we link against the main protobuf library which includes protoc
target_link_libraries(protoc-gen-bnet
  PRIVATE
    Trinity::Protobuf)

set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR})

install(TARGETS protoc-gen-bnet DESTINATION bin)
