cmake_minimum_required(VERSION 3.17)
project(sro_loader)

include(GenerateExportHeader)

set(CMAKE_CXX_STANDARD 14)
include_directories(${PROJECT_SOURCE_DIR}/detours/include)
link_directories(${PROJECT_SOURCE_DIR}/detours/lib.X86)

add_compile_definitions(_WINSOCK_DEPRECATED_NO_WARNINGS)
add_compile_definitions(_CRT_SECURE_NO_DEPRECATE)
add_compile_definitions(WIN32)
add_compile_definitions(_WINDOWS)
add_compile_definitions(_USRDLL)

add_definitions(-D_WIN32_WINNT=0x0502)

add_executable(injector src/injector.cpp
        src/blowfish.cpp src/common.cpp src/pk2reader.cpp src/shared_io.cpp src/shared_types.h src/silkroad.cpp)

add_library(sro_loader MODULE src/library.cpp
        src/blowfish.cpp src/common.cpp src/pk2reader.cpp src/shared_io.cpp src/shared_types.h src/silkroad.cpp)