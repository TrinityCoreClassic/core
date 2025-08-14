# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

# This file must be included via CMAKE_PROJECT_TOP_LEVEL_INCLUDES

# Configure dependency providers (CMake 3.24+)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
  # Dependency provider function
  function(trinity_dependency_provider method)
    if(method STREQUAL "FIND_PACKAGE")
      # Get find_package arguments
      cmake_parse_arguments(ARG "" "NAME;VERSION" "" ${ARGN})
      
      # Check if we should use bundled libraries
      if(FORCE_BUNDLED_LIBS OR (NOT PREFER_SYSTEM_LIBS))
        # Try FetchContent for known dependencies
        if(ARG_NAME STREQUAL "fmt" OR 
           ARG_NAME STREQUAL "Catch2" OR
           ARG_NAME STREQUAL "Protobuf")
          # Let FetchContent handle it
          return()
        endif()
      endif()
      
      # Otherwise, use standard find_package
      # Return without setting any result variables to use default behavior
      return()
    endif()
  endfunction()
  
  cmake_language(SET_DEPENDENCY_PROVIDER trinity_dependency_provider
    SUPPORTED_METHODS FIND_PACKAGE)
endif()