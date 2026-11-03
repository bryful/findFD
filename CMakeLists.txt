cmake_minimum_required(VERSION 3.15)
project(findFD LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVCで静的ランタイム (/MT) を使用する設定（CMake 3.15+）
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(findFD main.cpp)

if (MSVC)
    target_compile_options(findFD PRIVATE /utf-8 /W4)
endif()

