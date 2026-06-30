cmake_minimum_required(VERSION 3.10)
project(cppflags)

add_library(cppflags INTERFACE)
target_include_directories(cppflags INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
