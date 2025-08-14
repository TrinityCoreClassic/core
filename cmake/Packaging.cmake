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

# CPack general configuration
set(CPACK_PACKAGE_NAME "TrinityCore-Classic")
set(CPACK_PACKAGE_VENDOR "TrinityCore")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "TrinityCore Classic - MMORPG Framework")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/COPYING")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

# Installation directories
set(CPACK_PACKAGING_INSTALL_PREFIX "/opt/trinity-core/vanilla-classic")

# Package file name
set(CPACK_PACKAGE_FILE_NAME
  "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

# Source package configuration
set(CPACK_SOURCE_PACKAGE_FILE_NAME
  "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-Source")

# Files to exclude from source package
set(CPACK_SOURCE_IGNORE_FILES
  "/\\\\.git/"
  "/\\\\.github/"
  "/build/"
  "/install/"
  "/packages/"
  "\\\\.gitignore$"
  "\\\\.gitattributes$"
  "~$"
  "\\\\.swp$"
  "\\\\.#"
  "/#"
  "CMakeUserPresets\\\\.json$"
)

# Component configuration
set(CPACK_COMPONENTS_ALL
  Runtime
  Scripts
  Tools
  Configuration
  Development
  Debug
)

# Component descriptions
set(CPACK_COMPONENT_RUNTIME_DISPLAY_NAME "Server Binaries")
set(CPACK_COMPONENT_RUNTIME_DESCRIPTION
  "Core server executables (worldserver and bnetserver)")
set(CPACK_COMPONENT_RUNTIME_REQUIRED ON)

set(CPACK_COMPONENT_SCRIPTS_DISPLAY_NAME "Script Libraries")
set(CPACK_COMPONENT_SCRIPTS_DESCRIPTION
  "Dynamic script libraries for game content")
set(CPACK_COMPONENT_SCRIPTS_DEPENDS Runtime)

set(CPACK_COMPONENT_TOOLS_DISPLAY_NAME "Extraction Tools")
set(CPACK_COMPONENT_TOOLS_DESCRIPTION
  "Tools for extracting game data (maps, vmaps, mmaps)")

set(CPACK_COMPONENT_CONFIGURATION_DISPLAY_NAME "Configuration Files")
set(CPACK_COMPONENT_CONFIGURATION_DESCRIPTION
  "Server configuration file templates")

set(CPACK_COMPONENT_DEVELOPMENT_DISPLAY_NAME "Development Files")
set(CPACK_COMPONENT_DEVELOPMENT_DESCRIPTION
  "Headers and libraries for development")
set(CPACK_COMPONENT_DEVELOPMENT_DISABLED ON)

set(CPACK_COMPONENT_DEBUG_DISPLAY_NAME "Debug Symbols")
set(CPACK_COMPONENT_DEBUG_DESCRIPTION
  "Debug symbols for troubleshooting")
set(CPACK_COMPONENT_DEBUG_DISABLED ON)

# Component groups
set(CPACK_COMPONENT_RUNTIME_GROUP "Server")
set(CPACK_COMPONENT_SCRIPTS_GROUP "Server")
set(CPACK_COMPONENT_TOOLS_GROUP "Tools")
set(CPACK_COMPONENT_CONFIGURATION_GROUP "Server")
set(CPACK_COMPONENT_DEVELOPMENT_GROUP "Development")
set(CPACK_COMPONENT_DEBUG_GROUP "Development")

set(CPACK_COMPONENT_GROUP_SERVER_DISPLAY_NAME "TrinityCore Server")
set(CPACK_COMPONENT_GROUP_SERVER_DESCRIPTION
  "Core server components required to run TrinityCore")

set(CPACK_COMPONENT_GROUP_TOOLS_DISPLAY_NAME "Data Extraction Tools")
set(CPACK_COMPONENT_GROUP_TOOLS_DESCRIPTION
  "Tools for extracting required data from the game client")

set(CPACK_COMPONENT_GROUP_DEVELOPMENT_DISPLAY_NAME "Development Components")
set(CPACK_COMPONENT_GROUP_DEVELOPMENT_DESCRIPTION
  "Components for developing TrinityCore")

# Platform-specific configurations
if(WIN32)
  # Windows-specific settings
  set(CPACK_GENERATOR "ZIP;NSIS")
  
  # NSIS specific
  set(CPACK_NSIS_DISPLAY_NAME "TrinityCore Classic ${PROJECT_VERSION}")
  set(CPACK_NSIS_PACKAGE_NAME "TrinityCore Classic")
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
  set(CPACK_NSIS_MODIFY_PATH ON)
  
  # Create shortcuts
  set(CPACK_NSIS_CREATE_ICONS_EXTRA
    "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\worldserver.lnk' '$INSTDIR\\\\bin\\\\worldserver.exe'"
    "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\bnetserver.lnk' '$INSTDIR\\\\bin\\\\bnetserver.exe'"
  )
  set(CPACK_NSIS_DELETE_ICONS_EXTRA
    "Delete '$SMPROGRAMS\\\\$START_MENU\\\\worldserver.lnk'"
    "Delete '$SMPROGRAMS\\\\$START_MENU\\\\bnetserver.lnk'"
  )
  
elseif(APPLE)
  # macOS-specific settings
  set(CPACK_GENERATOR "TGZ;DragNDrop")
  set(CPACK_DMG_VOLUME_NAME "TrinityCore Classic ${PROJECT_VERSION}")
  set(CPACK_DMG_FORMAT "UDZO")
  set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_SOURCE_DIR}/cmake/packaging/dmg-background.png")
  
elseif(UNIX)
  # Linux/Unix-specific settings
  set(CPACK_GENERATOR "TGZ;DEB;RPM")
  
  # Debian package settings
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "TrinityCore Team")
  set(CPACK_DEBIAN_PACKAGE_SECTION "games")
  set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libmysqlclient21 | libmariadb3, "
    "libssl3 | libssl1.1, "
    "libboost-system1.74.0, "
    "libboost-filesystem1.74.0, "
    "libboost-thread1.74.0, "
    "libboost-program-options1.74.0, "
    "libreadline8"
  )
  set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}")
  set(CPACK_DEBIAN_COMPRESSION_TYPE "xz")
  
  # RPM package settings
  set(CPACK_RPM_PACKAGE_LICENSE "GPL-2.0")
  set(CPACK_RPM_PACKAGE_GROUP "Amusements/Games")
  set(CPACK_RPM_PACKAGE_REQUIRES
    "mysql-libs >= 8.0, "
    "openssl-libs >= 3.0, "
    "boost-system >= 1.74, "
    "boost-filesystem >= 1.74, "
    "boost-thread >= 1.74, "
    "boost-program-options >= 1.74, "
    "readline"
  )
  set(CPACK_RPM_PACKAGE_ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}")
  set(CPACK_RPM_COMPRESSION_TYPE "xz")
  
  # Create system service files
  configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/packaging/systemd/trinitycore-world.service.in"
    "${CMAKE_BINARY_DIR}/trinitycore-world.service"
    @ONLY
  )
  configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/packaging/systemd/trinitycore-bnet.service.in"
    "${CMAKE_BINARY_DIR}/trinitycore-bnet.service"
    @ONLY
  )
  
  # Install service files
  install(FILES
    "${CMAKE_BINARY_DIR}/trinitycore-world.service"
    "${CMAKE_BINARY_DIR}/trinitycore-bnet.service"
    DESTINATION lib/systemd/system
    COMPONENT Configuration
  )
endif()

# Archive generators settings
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)

# Custom installation rules for packaging
install(FILES
  "${CMAKE_SOURCE_DIR}/COPYING"
  "${CMAKE_SOURCE_DIR}/README.md"
  "${CMAKE_SOURCE_DIR}/AUTHORS"
  "${CMAKE_SOURCE_DIR}/THANKS"
  DESTINATION "${CMAKE_INSTALL_PREFIX}"
  COMPONENT Runtime
)

# Create a version file
file(WRITE "${CMAKE_BINARY_DIR}/VERSION"
  "TrinityCore Classic ${PROJECT_VERSION}\n"
  "Build Date: ${CMAKE_BUILD_DATE}\n"
  "Git Revision: ${GIT_REVISION}\n"
  "Git Branch: ${GIT_BRANCH}\n"
)

install(FILES
  "${CMAKE_BINARY_DIR}/VERSION"
  DESTINATION "${CMAKE_INSTALL_PREFIX}"
  COMPONENT Runtime
)

# Helper function to create component packages
function(trinity_add_package_component component)
  set(options REQUIRED DISABLED HIDDEN)
  set(oneValueArgs DISPLAY_NAME DESCRIPTION GROUP)
  set(multiValueArgs DEPENDS INSTALL_TYPES)
  
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
  
  if(ARG_DISPLAY_NAME)
    set(CPACK_COMPONENT_${component}_DISPLAY_NAME ${ARG_DISPLAY_NAME} PARENT_SCOPE)
  endif()
  
  if(ARG_DESCRIPTION)
    set(CPACK_COMPONENT_${component}_DESCRIPTION ${ARG_DESCRIPTION} PARENT_SCOPE)
  endif()
  
  if(ARG_GROUP)
    set(CPACK_COMPONENT_${component}_GROUP ${ARG_GROUP} PARENT_SCOPE)
  endif()
  
  if(ARG_DEPENDS)
    set(CPACK_COMPONENT_${component}_DEPENDS ${ARG_DEPENDS} PARENT_SCOPE)
  endif()
  
  if(ARG_REQUIRED)
    set(CPACK_COMPONENT_${component}_REQUIRED ON PARENT_SCOPE)
  endif()
  
  if(ARG_DISABLED)
    set(CPACK_COMPONENT_${component}_DISABLED ON PARENT_SCOPE)
  endif()
  
  if(ARG_HIDDEN)
    set(CPACK_COMPONENT_${component}_HIDDEN ON PARENT_SCOPE)
  endif()
endfunction()

# Include CPack
include(CPack)

# Add custom package targets
add_custom_target(package-server
  COMMAND ${CMAKE_CPACK_COMMAND} -G ${CPACK_GENERATOR} -C $<CONFIG> -D CPACK_COMPONENTS_ALL="Runtime;Scripts;Configuration"
  COMMENT "Creating server package..."
)

add_custom_target(package-tools
  COMMAND ${CMAKE_CPACK_COMMAND} -G ${CPACK_GENERATOR} -C $<CONFIG> -D CPACK_COMPONENTS_ALL="Tools"
  COMMENT "Creating tools package..."
)

add_custom_target(package-dev
  COMMAND ${CMAKE_CPACK_COMMAND} -G ${CPACK_GENERATOR} -C $<CONFIG> -D CPACK_COMPONENTS_ALL="Development;Debug"
  COMMENT "Creating development package..."
)