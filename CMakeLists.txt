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

add_compile_definitions(LIBSODIUM_FAIL_CODE=-1 ENCRYPT_FAIL_CODE=-2 OFSTREAM_FAIL_CODE=-3 OFSTREAM_WRITE_FAIL=-4 PAK_DIR_FAIL_CODE=-5 TELLG_FAIL_CODE=-6 IFSTREAM_FAIL_CODE=-7 IFSTREAM_FAIL_READ_CODE=-8 OPEN_FAIL_CODE=-9 FSTAT_FAIL_CODE=-10 MMAP_FAIL_CODE=-11 UN_MAP_FAIL_CODE=-12 DECRYPT_FAIL_CODE=-13 VECTOR_RESIZE_FAIL_CODE=-14 TELLP_FAIL_CODE=-15)

set(CMAKE_CXX_STANDARD 17)

find_package(lz4 CONFIG REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)

add_library(RPK STATIC)

target_sources(RPK 
    PRIVATE 
     src/RPKPacker.cpp 
     src/RPKLoader.cpp
    
    PUBLIC
     FILE_SET HEADERS
     FILES
      include/RPKPacker.hpp
      include/RPKLoader.hpp
  )

target_link_libraries(RPK PUBLIC lz4::lz4 unofficial-sodium::sodium)
target_include_directories(RPK PUBLIC include)

if(CLI)
    add_executable(Packer)

    target_sources(Packer
        PRIVATE
         src/RPKPacker.cpp
    )

    target_link_libraries(Packer lz4::lz4 unofficial-sodium::sodium)

    target_compile_definitions(Packer PRIVATE CLI)
    target_include_directories(Packer PUBLIC include)
endif()

if(FULLCLI)
    add_executable(Unpacker)

    target_sources(Unpacker
        PRIVATE
         src/RPKLoader.cpp
     )

     target_link_libraries(Unpacker lz4::lz4 unofficial-sodium::sodium)

     target_compile_definitions(Unpacker PRIVATE CLI)

     target_include_directories(Unpacker PUBLIC include)
endif()
