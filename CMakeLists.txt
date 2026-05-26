cmake_minimum_required(VERSION 3.16)
project(ConcaveHullProj LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(OpenCV REQUIRED)

add_executable(concave_hull
    src/main.cpp
    src/pos_reader.cpp
    src/concave_hull.cpp
    src/output.cpp
    src/tests.cpp
)
target_include_directories(concave_hull PRIVATE src)
target_link_libraries(concave_hull PRIVATE ${OpenCV_LIBS})
