cmake_minimum_required(VERSION 3.20)
project(ZumbiNativeEngine VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(zumbi_native
    src/main.cpp
    src/asset_archive.cpp
    src/zs_compiler.cpp
)
target_include_directories(zumbi_native PRIVATE include)

if(WIN32)
    target_link_libraries(zumbi_native PRIVATE bcrypt)
else()
    find_package(OpenSSL REQUIRED)
    target_link_libraries(zumbi_native PRIVATE OpenSSL::Crypto)
endif()

if(MSVC)
    target_compile_options(zumbi_native PRIVATE /W4 /permissive-)
else()
    target_compile_options(zumbi_native PRIVATE -Wall -Wextra -Wpedantic)
endif()
