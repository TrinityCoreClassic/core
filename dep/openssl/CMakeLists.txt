# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.


# Assume OpenSSL was resolved in the top-level CMakeLists.txt
message(STATUS "✔ OpenSSL already resolved by top-level CMakeLists.txt")

add_library(openssl INTERFACE)

target_include_directories(openssl INTERFACE ${OPENSSL_INCLUDE_DIR})

target_link_libraries(openssl
  INTERFACE
    ${OPENSSL_SSL_LIBRARY}
    ${OPENSSL_CRYPTO_LIBRARY})
