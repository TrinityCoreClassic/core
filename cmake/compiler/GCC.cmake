# GCC-specific compiler settings

target_compile_options(trinity-compiler-options INTERFACE
  -fno-delete-null-pointer-checks
  -fno-strict-aliasing
  -pipe
)

# Enable specific warnings
target_compile_options(trinity-warning-extra INTERFACE
  -Wno-missing-field-initializers
  -Wno-maybe-uninitialized
)

# GCC-specific optimizations
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  target_compile_options(trinity-release-options INTERFACE
    -fomit-frame-pointer
    -ftree-vectorize
    -ffast-math
    -funroll-loops
    -ffinite-math-only
    -fno-math-errno
    -funsafe-math-optimizations
  )
endif()

# Modern CPU optimizations (can be overridden with TRINITY_CPU_TARGET)
if(NOT DEFINED TRINITY_CPU_TARGET)
  set(TRINITY_CPU_TARGET "native" CACHE STRING "Target CPU architecture (native, x86-64-v2, x86-64-v3, etc.)")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
  if(TRINITY_CPU_TARGET STREQUAL "native")
    target_compile_options(trinity-release-options INTERFACE -march=native -mtune=native)
  else()
    target_compile_options(trinity-release-options INTERFACE -march=${TRINITY_CPU_TARGET} -mtune=${TRINITY_CPU_TARGET})
  endif()
endif()

# Link-time optimization
if(CMAKE_INTERPROCEDURAL_OPTIMIZATION)
  target_compile_options(trinity-compiler-options INTERFACE -flto=auto)
  target_link_options(trinity-compiler-options INTERFACE -flto=auto)
endif()

# Security hardening
target_compile_options(trinity-compiler-options INTERFACE
  -fstack-protector-strong
  -fstack-clash-protection
  -fcf-protection=full
  -D_FORTIFY_SOURCE=2
)

# Additional hardening for release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
  target_compile_options(trinity-compiler-options INTERFACE
    -D_GLIBCXX_ASSERTIONS
  )
  target_link_options(trinity-compiler-options INTERFACE
    -Wl,-z,relro
    -Wl,-z,now
    -Wl,-z,noexecstack
  )
endif()

# Enable colored output
if(CMAKE_COLOR_DIAGNOSTICS)
  target_compile_options(trinity-compiler-options INTERFACE
    -fdiagnostics-color=always
  )
endif()