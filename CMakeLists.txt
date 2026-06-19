cmake_minimum_required(VERSION 3.17)

if(BUILD_NAO)
  set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/Make/CMake/NaoToolchain.cmake")
endif()

project(SabanaHerons2026 LANGUAGES CXX)

# Keep the existing build system under Make/CMake as the single source of truth.
add_subdirectory(Make/CMake)
