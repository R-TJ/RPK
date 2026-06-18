cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VCPKG_ROOT})
        set(CMAKE_TOOLCHAIN_FILE
            "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "")
    else()
        message(FATAL_ERROR "VCPKG_ROOT is not set")
    endif()
endif()

option(CLI "Compile to archive from the terminal" ON)
option(FULLCLI "Compile to unarchive from the terminal" ON)

project(RPK LANGUAGES CXX)

# Error codes
add_compile_definitions(IFSTREAM_FAIL=-1 IFSTREAM_READ_FAIL=-2 LIBSODIUM_FAIL_CODE=-3 LZ4_FAIL=-4 OFSTREAM_FAIL=-5 OFSTREAM_WRITE_FAIL=-6)

# Misc definitions
add_compile_definitions(MAX_OPEN_FILES=16 SPEED_CHECK_DURATION=5)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(lz4 CONFIG REQUIRED)
find_package(libsodium CONFIG REQUIRED)

add_library(RPK STATIC)

target_sources(RPK 
    PRIVATE 
     src/Packer.cpp 
     src/Loader.cpp
    
    PUBLIC
     FILE_SET HEADERS
     FILES
      include/Packer.hpp
      include/Loader.hpp
  )

target_link_libraries(RPK PUBLIC lz4::lz4 libsodium::sodium)
target_include_directories(RPK PUBLIC include)

if(CLI)
    add_executable(Packer)

    target_sources(Packer
        PRIVATE
         src/Packer.cpp
    )

    target_link_libraries(Packer lz4::lz4 libsodium::sodium)

    target_compile_definitions(Packer PRIVATE CLI)
    target_include_directories(Packer PUBLIC include)
endif()

if(FULLCLI)
    add_executable(Unpacker)

    target_sources(Unpacker
        PRIVATE
         src/Loader.cpp
     )

     target_link_libraries(Unpacker lz4::lz4 libsodium::sodium)

     target_compile_definitions(Unpacker PRIVATE CLI)

     target_include_directories(Unpacker PUBLIC include)
endif()
