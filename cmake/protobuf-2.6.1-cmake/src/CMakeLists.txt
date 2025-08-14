# CMake build file for protobuf 2.6.1 libraries
# Based on analysis of src/Makefile.am

# Helper function to add protobuf library
function(add_protobuf_library name)
  add_library(${name} ${ARGN})
  # Ensure config.h can be found
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  # Add compile definitions for va_copy support
  target_compile_definitions(${name} PRIVATE _GNU_SOURCE)
  if(CMAKE_THREAD_LIBS_INIT)
    target_link_libraries(${name} ${CMAKE_THREAD_LIBS_INIT})
  endif()
  if(ZLIB_FOUND)
    target_link_libraries(${name} ${ZLIB_LIBRARIES})
  endif()
  set_target_properties(${name} PROPERTIES
    OUTPUT_NAME ${name}
    DEBUG_POSTFIX "d"
  )
endfunction()

# libprotobuf-lite library (minimal runtime)
set(LIBPROTOBUF_LITE_SOURCES
  google/protobuf/stubs/common.cc
  google/protobuf/stubs/once.cc
  google/protobuf/stubs/stringprintf.cc
  google/protobuf/extension_set.cc
  google/protobuf/generated_message_util.cc
  google/protobuf/message_lite.cc
  google/protobuf/repeated_field.cc
  google/protobuf/wire_format_lite.cc
  google/protobuf/io/coded_stream.cc
  google/protobuf/io/zero_copy_stream.cc
  google/protobuf/io/zero_copy_stream_impl_lite.cc
)

# Additional platform-specific sources
if(WIN32)
  list(APPEND LIBPROTOBUF_LITE_SOURCES
    google/protobuf/stubs/atomicops_internals_x86_msvc.cc
  )
else()
  list(APPEND LIBPROTOBUF_LITE_SOURCES
    google/protobuf/stubs/atomicops_internals_x86_gcc.cc
  )
endif()

add_protobuf_library(protobuf-lite STATIC ${LIBPROTOBUF_LITE_SOURCES})

# libprotobuf library (full runtime with reflection)
set(LIBPROTOBUF_SOURCES
  ${LIBPROTOBUF_LITE_SOURCES}
  google/protobuf/stubs/strutil.cc
  google/protobuf/stubs/substitute.cc
  google/protobuf/stubs/structurally_valid.cc
  google/protobuf/descriptor.cc
  google/protobuf/descriptor.pb.cc
  google/protobuf/descriptor_database.cc
  google/protobuf/dynamic_message.cc
  google/protobuf/extension_set_heavy.cc
  google/protobuf/generated_message_reflection.cc
  google/protobuf/message.cc
  google/protobuf/reflection_ops.cc
  google/protobuf/service.cc
  google/protobuf/text_format.cc
  google/protobuf/unknown_field_set.cc
  google/protobuf/wire_format.cc
  google/protobuf/io/gzip_stream.cc
  google/protobuf/io/printer.cc
  google/protobuf/io/strtod.cc
  google/protobuf/io/tokenizer.cc
  google/protobuf/io/zero_copy_stream_impl.cc
  google/protobuf/compiler/importer.cc
  google/protobuf/compiler/parser.cc
)

add_protobuf_library(protobuf STATIC ${LIBPROTOBUF_SOURCES})

# libprotoc library (compiler library)
set(LIBPROTOC_SOURCES
  google/protobuf/compiler/code_generator.cc
  google/protobuf/compiler/command_line_interface.cc
  google/protobuf/compiler/plugin.cc
  google/protobuf/compiler/plugin.pb.cc
  google/protobuf/compiler/subprocess.cc
  google/protobuf/compiler/zip_writer.cc
  google/protobuf/compiler/cpp/cpp_enum.cc
  google/protobuf/compiler/cpp/cpp_enum_field.cc
  google/protobuf/compiler/cpp/cpp_extension.cc
  google/protobuf/compiler/cpp/cpp_field.cc
  google/protobuf/compiler/cpp/cpp_file.cc
  google/protobuf/compiler/cpp/cpp_generator.cc
  google/protobuf/compiler/cpp/cpp_helpers.cc
  google/protobuf/compiler/cpp/cpp_message.cc
  google/protobuf/compiler/cpp/cpp_message_field.cc
  google/protobuf/compiler/cpp/cpp_primitive_field.cc
  google/protobuf/compiler/cpp/cpp_service.cc
  google/protobuf/compiler/cpp/cpp_string_field.cc
  google/protobuf/compiler/java/java_context.cc
  google/protobuf/compiler/java/java_enum.cc
  google/protobuf/compiler/java/java_enum_field.cc
  google/protobuf/compiler/java/java_extension.cc
  google/protobuf/compiler/java/java_field.cc
  google/protobuf/compiler/java/java_file.cc
  google/protobuf/compiler/java/java_generator.cc
  google/protobuf/compiler/java/java_generator_factory.cc
  google/protobuf/compiler/java/java_helpers.cc
  google/protobuf/compiler/java/java_lazy_message_field.cc
  google/protobuf/compiler/java/java_message.cc
  google/protobuf/compiler/java/java_message_field.cc
  google/protobuf/compiler/java/java_name_resolver.cc
  google/protobuf/compiler/java/java_primitive_field.cc
  google/protobuf/compiler/java/java_shared_code_generator.cc
  google/protobuf/compiler/java/java_service.cc
  google/protobuf/compiler/java/java_string_field.cc
  google/protobuf/compiler/java/java_doc_comment.cc
  google/protobuf/compiler/python/python_generator.cc
)

add_protobuf_library(protoc STATIC ${LIBPROTOC_SOURCES})
target_link_libraries(protoc protobuf)

# protoc executable
add_executable(protoc_exe google/protobuf/compiler/main.cc)
target_link_libraries(protoc_exe protoc protobuf)
set_target_properties(protoc_exe PROPERTIES
  OUTPUT_NAME protoc
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Installation
install(TARGETS protobuf-lite protobuf protoc
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

install(TARGETS protoc_exe
  RUNTIME DESTINATION bin
)