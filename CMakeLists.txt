
if (UNIX)
  add_definitions(-Wno-sign-compare -Wno-enum-compare)
endif()

cmake_minimum_required(VERSION 3.10)
project(svger)

set(cpprestsdk_DIR /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/cmake/)

find_package(PkgConfig REQUIRED)
find_package(Boost COMPONENTS system filesystem REQUIRED)
find_package(cpprestsdk REQUIRED)
pkg_check_modules(RSVG REQUIRED IMPORTED_TARGET librsvg-2.0>=2.35)
pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET cairo)

add_executable(svger main.cpp server.cpp)
target_link_libraries(svger PRIVATE cpprestsdk::cpprest PkgConfig::RSVG PkgConfig::CAIRO ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY})
