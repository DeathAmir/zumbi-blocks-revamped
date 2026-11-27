cmake_minimum_required(VERSION 3.20)
project(ZumbiNativeEngine VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(zumbi_native
    src/main.cpp
    src/engine_core.cpp
    src/game_runtime.cpp
    src/asset_archive.cpp
    src/asset_package.cpp
    src/zs_compiler.cpp
)
target_include_directories(zumbi_native PRIVATE include)

if(WIN32)
    if(NOT OPENSSL_INCLUDE_DIR OR NOT EXISTS "${OPENSSL_INCLUDE_DIR}/openssl/evp.h")
        message(FATAL_ERROR "Precompiled OpenSSL headers are required on Windows.")
    endif()
    if(NOT OPENSSL_CRYPTO_LIBRARY OR NOT EXISTS "${OPENSSL_CRYPTO_LIBRARY}")
        message(FATAL_ERROR "Precompiled OpenSSL crypto import library is required on Windows.")
    endif()
    if(NOT OPENSSL_CRYPTO_DLL OR NOT EXISTS "${OPENSSL_CRYPTO_DLL}")
        message(FATAL_ERROR "Precompiled OpenSSL crypto DLL is required on Windows.")
    endif()
    if(NOT ZLIB_INCLUDE_DIR OR NOT EXISTS "${ZLIB_INCLUDE_DIR}/zlib.h")
        message(FATAL_ERROR "Precompiled zlib headers are required on Windows.")
    endif()
    if(NOT ZLIB_LIBRARY_RELEASE OR NOT EXISTS "${ZLIB_LIBRARY_RELEASE}")
        message(FATAL_ERROR "Precompiled zlib release library is required on Windows.")
    endif()
    add_library(OpenSSL::Crypto SHARED IMPORTED GLOBAL)
    set_target_properties(OpenSSL::Crypto PROPERTIES
        IMPORTED_CONFIGURATIONS RELEASE
        IMPORTED_IMPLIB_RELEASE "${OPENSSL_CRYPTO_LIBRARY}"
        IMPORTED_LOCATION_RELEASE "${OPENSSL_CRYPTO_DLL}"
        INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
    )
    add_library(ZLIB::ZLIB UNKNOWN IMPORTED GLOBAL)
    set_target_properties(ZLIB::ZLIB PROPERTIES
        IMPORTED_CONFIGURATIONS RELEASE
        IMPORTED_LOCATION_RELEASE "${ZLIB_LIBRARY_RELEASE}"
        INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIR}"
    )
else()
    find_package(OpenSSL REQUIRED COMPONENTS Crypto)
    find_package(ZLIB REQUIRED)
endif()
target_link_libraries(zumbi_native PRIVATE OpenSSL::Crypto ZLIB::ZLIB)

if(WIN32)
    target_link_libraries(zumbi_native PRIVATE bcrypt)
endif()

if(MSVC)
    target_compile_options(zumbi_native PRIVATE /W4 /permissive-)
else()
    target_compile_options(zumbi_native PRIVATE -Wall -Wextra -Wpedantic)
endif()

set(ZBR_ASSET_PUBLIC_KEY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/keys/asset_signing_public_key.hex")
if(EXISTS "${ZBR_ASSET_PUBLIC_KEY_FILE}")
    file(READ "${ZBR_ASSET_PUBLIC_KEY_FILE}" ZBR_ASSET_PUBLIC_KEY_HEX)
    string(STRIP "${ZBR_ASSET_PUBLIC_KEY_HEX}" ZBR_ASSET_PUBLIC_KEY_HEX)
else()
    set(ZBR_ASSET_PUBLIC_KEY_HEX "")
endif()
target_compile_definitions(zumbi_native PRIVATE ZBR_ASSET_PUBLIC_KEY_HEX="${ZBR_ASSET_PUBLIC_KEY_HEX}")
