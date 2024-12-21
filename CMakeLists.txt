cmake_minimum_required(VERSION 3.29)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_definitions(
  "UNICODE"
  "_UNICODE"
)
set(
  CMAKE_MSVC_RUNTIME_LIBRARY
  "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

set(
  CMAKE_TOOLCHAIN_FILE
  "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
  CACHE STRING "Vcpkg toolchain file"
)

# We need C for md4c
project(Vicius VERSION 1.0.0 LANGUAGES C CXX)

if(MSVC)
  add_compile_options(
    # Standard C++ exception behavior
    "/EHsc"
    # Include content and marker in error messages
    "/diagnostics:caret"
    # Source is UTF-8
    "/utf-8"
  )
endif()

add_subdirectory(dll)
add_subdirectory(src)