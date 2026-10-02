cmake_minimum_required(VERSION 3.20)
project(ForgeOSWindows LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(ironbox ForgeOS.cpp)

if(MSVC)
  target_compile_options(ironbox PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()

target_compile_definitions(ironbox PRIVATE FORGEOS_WIN32=1)
target_link_libraries(ironbox PRIVATE user32 gdi32 winmm ws2_32)
