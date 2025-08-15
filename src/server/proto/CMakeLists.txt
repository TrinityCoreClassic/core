# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

# Optional protobuf generation
if(WITH_PROTO_GENERATION)
  # Find protoc-gen-bnet plugin
  find_program(PROTOC_GEN_BNET protoc-gen-bnet
    PATHS
      ${CMAKE_SOURCE_DIR}/contrib/protoc-bnet/bin
      ${CMAKE_BINARY_DIR}/contrib/protoc-bnet
      /usr/local/bin
      /usr/bin
  )
  
  if(NOT PROTOC_GEN_BNET)
    message(FATAL_ERROR "protoc-gen-bnet not found. Build it from contrib/protoc-bnet or disable WITH_PROTO_GENERATION")
  endif()
  
  # Find all .proto files
  file(GLOB_RECURSE PROTO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.proto)
  
  # Generate protobuf files
  foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(PROTO_DIR ${PROTO_FILE} DIRECTORY)
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
    
    # Run protoc with the bnet plugin
    execute_process(
      COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        --plugin=protoc-gen-bnet=${PROTOC_GEN_BNET}
        --bnet_out=dllexport_decl=TC_PROTO_API:${PROTO_DIR}
        -I${CMAKE_CURRENT_SOURCE_DIR}
        ${PROTO_FILE}
      RESULT_VARIABLE PROTO_RESULT
    )
    
    if(NOT PROTO_RESULT EQUAL 0)
      message(WARNING "Failed to generate protobuf files for ${PROTO_FILE}")
    endif()
  endforeach()
  
  message(STATUS "Protobuf files regenerated")
endif()

CollectSourceFiles(
  ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE_SOURCES
  # Exclude
  ${CMAKE_CURRENT_SOURCE_DIR}/PrecompiledHeaders)

if (USE_COREPCH)
  set(PRIVATE_PCH_HEADER PrecompiledHeaders/protoPCH.h)
endif()

GroupSources(${CMAKE_CURRENT_SOURCE_DIR})

add_definitions(-DTRINITY_API_EXPORT_PROTO)

add_library(proto
  ${PRIVATE_SOURCES}
)

CollectIncludeDirectories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  PUBLIC_INCLUDES
  # Exclude
  ${CMAKE_CURRENT_SOURCE_DIR}/PrecompiledHeaders
  ${CMAKE_CURRENT_SOURCE_DIR}/Client/api
  ${CMAKE_CURRENT_SOURCE_DIR}/Client/global_extensions)

target_include_directories(proto
  PUBLIC
    ${PUBLIC_INCLUDES}
  PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(proto
  PRIVATE
    trinity-default-interface
    trinity-no-warning-interface
  PUBLIC
    common
    Trinity::Protobuf)

set_target_properties(proto
  PROPERTIES
    FOLDER
      "server")

if( BUILD_SHARED_LIBS )
  if( UNIX )
    install(TARGETS proto
      LIBRARY
        DESTINATION lib)
  elseif( WIN32 )
    install(TARGETS proto
      RUNTIME
        DESTINATION "${CMAKE_INSTALL_PREFIX}")
  endif()
endif()

# Generate precompiled header
if (USE_COREPCH)
  add_cxx_pch(proto ${PRIVATE_PCH_HEADER})
endif()
