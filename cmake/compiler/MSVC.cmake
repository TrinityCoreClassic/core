# MSVC-specific compiler settings

# Basic MSVC options
target_compile_options(trinity-compiler-options INTERFACE
  /MP           # Multi-processor compilation
  /bigobj       # Increase object file sections
  /permissive-  # Strict conformance mode
  /Zc:__cplusplus # Correct __cplusplus macro
  /utf-8        # Set source and execution character sets to UTF-8
)

# Runtime library selection
set(MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" 
  CACHE STRING "MSVC runtime library")
set_property(CACHE MSVC_RUNTIME_LIBRARY PROPERTY STRINGS
  "MultiThreaded"
  "MultiThreadedDebug"
  "MultiThreadedDLL"
  "MultiThreadedDebugDLL"
)

# Set runtime library
set(CMAKE_MSVC_RUNTIME_LIBRARY ${MSVC_RUNTIME_LIBRARY})

# Enable specific warnings
target_compile_options(trinity-warning-extra INTERFACE
  /wd4996  # Disable deprecated warnings
  /wd4244  # Disable conversion warnings
  /wd4267  # Disable size_t to int conversion warnings
)

# Debug options
target_compile_options(trinity-debug-options INTERFACE
  $<$<CONFIG:Debug>:/RTC1>  # Runtime checks
  $<$<CONFIG:Debug>:/GS>    # Security checks
  $<$<CONFIG:Debug>:/sdl>   # Additional security checks
)

# Release optimizations
target_compile_options(trinity-release-options INTERFACE
  $<$<CONFIG:Release>:/Oi>  # Enable intrinsic functions
  $<$<CONFIG:Release>:/Ot>  # Favor fast code
  $<$<CONFIG:Release>:/Oy>  # Omit frame pointers
  $<$<CONFIG:Release>:/GT>  # Enable fiber-safe optimizations
  $<$<CONFIG:Release>:/GF>  # Enable string pooling
  $<$<CONFIG:Release>:/GS->  # Disable security checks in release
)

# Link-time optimization
if(CMAKE_INTERPROCEDURAL_OPTIMIZATION)
  target_compile_options(trinity-compiler-options INTERFACE /GL)
  target_link_options(trinity-compiler-options INTERFACE /LTCG)
endif()

# Enable Just My Code debugging
if(MSVC_VERSION GREATER_EQUAL 1914) # Visual Studio 2017 15.7+
  target_compile_options(trinity-debug-options INTERFACE
    $<$<CONFIG:Debug>:/JMC>
  )
endif()

# Enable Address Sanitizer (Visual Studio 2019 16.9+)
if(MSVC_VERSION GREATER_EQUAL 1929)
  target_compile_options(trinity-asan-options INTERFACE /fsanitize=address)
endif()

# Set warning level for MSVC
if(CMAKE_COMPILE_WARNING_AS_ERROR)
  target_compile_options(trinity-warning-default INTERFACE /WX)
  target_compile_options(trinity-warning-extra INTERFACE /WX)
  target_compile_options(trinity-warning-all INTERFACE /WX)
endif()

# Parallel build
if(NOT DEFINED ENV{CL})
  set(ENV{CL} /MP)
endif()