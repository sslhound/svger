
if (UNIX)
  add_definitions(-Wno-sign-compare -Wno-enum-compare)
endif()

cmake_minimum_required(VERSION 3.10)
project(svger CXX)

set(CMAKE_BUILD_TYPE Debug)
set(cpprestsdk_DIR /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/cmake/)
set(prometheus-cpp_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendr/prometheus-cpp-master)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall")

find_package(PkgConfig REQUIRED)
find_package(Boost COMPONENTS system REQUIRED)
find_package(cpprestsdk REQUIRED)
find_package(prometheus-cpp REQUIRED)
pkg_check_modules(RSVG REQUIRED IMPORTED_TARGET librsvg-2.0>=2.35)
pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET cairo)

add_executable(svger main.cpp server.cpp)
target_include_directories(svger PRIVATE include)
target_link_libraries(svger PRIVATE cpprestsdk::cpprest PkgConfig::RSVG PkgConfig::CAIRO ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY} prometheus-cpp::core)
