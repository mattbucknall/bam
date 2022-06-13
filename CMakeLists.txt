cmake_minimum_required(VERSION 3.22)
project(bam C)

find_package(PkgConfig)
pkg_check_modules(SDL2 REQUIRED sdl2)

set(CMAKE_C_STANDARD 99)

include_directories("${CMAKE_SOURCE_DIR}")

add_subdirectory(demo)
