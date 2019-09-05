cmake_minimum_required(VERSION 3.10)
project(svger)

find_package(PkgConfig REQUIRED)
pkg_check_modules(RSVG REQUIRED IMPORTED_TARGET librsvg-2.0>=2.35)
pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET cairo)

add_executable(svger main.cpp)
target_link_libraries(svger PkgConfig::RSVG PkgConfig::CAIRO)
