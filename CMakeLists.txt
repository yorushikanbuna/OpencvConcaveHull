cmake_minimum_required(VERSION 3.16)
project(ConcaveHullProj LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(OpenCV REQUIRED)

add_executable(concave_hull concave_hull.cpp)
target_link_libraries(concave_hull PRIVATE ${OpenCV_LIBS})
