# CMake Build System for Protocol Buffers 2.6.1

This directory contains CMake build files that replace the autotools-based build system of protobuf 2.6.1.

## Purpose

The original protobuf 2.6.1 uses autotools (autoconf, automake, libtool) which:
- Requires additional dependencies on all platforms
- Is particularly problematic on Windows
- Doesn't integrate well with modern CMake projects

This CMake-based build system provides:
- Cross-platform compatibility (Linux, macOS, Windows)
- No autotools dependencies
- Better integration with CMake projects
- Faster parallel builds
- Proper IDE support

## What Gets Built

The CMake files build the same targets as the original Makefile.am:

1. **libprotobuf-lite** - Minimal runtime library
2. **libprotobuf** - Full runtime with reflection support
3. **libprotoc** - Compiler support library
4. **protoc** - The protocol buffer compiler

## How It Works

The parent CMakeLists.txt applies these files as a patch after downloading protobuf 2.6.1:

```cmake
PATCH_COMMAND ${CMAKE_COMMAND} -E copy_directory 
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake-build/protobuf-2.6.1-cmake/
    <SOURCE_DIR>/
```

This adds CMakeLists.txt files without modifying any source code.

## Differences from Autotools Build

- Builds static libraries by default (same as our autotools configuration)
- Skips building tests and examples by default
- Uses CMake's standard installation paths
- Automatically handles platform differences (Windows vs Unix)

## File Mapping

- `CMakeLists.txt` - Replaces `configure.ac`
- `src/CMakeLists.txt` - Replaces `src/Makefile.am`
- Source file lists extracted from original `Makefile.am`