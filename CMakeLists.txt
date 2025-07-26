cmake_minimum_required(VERSION 3.29...3.31 FATAL_ERROR)

# Suppress incorrect unused variable warning
if (CMAKE_TOOLCHAIN_FILE)
  message(STATUS "CMAKE_TOOLCHAIN_FILE: ${CMAKE_TOOLCHAIN_FILE}")
endif ()

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
  # MSBuild defaults to these, but CMake doesn't, at least in `RelWithDebInfo` builds
  # These all make the binary *much* smaller
  add_link_options(
    # Incremental linking speeds up rebuilds, which we don't care about for release
    "$<$<NOT:$<CONFIG:Debug>>:/INCREMENTAL:NO>"
    # COMDAT folding; drops off another 1MB
    "$<$<NOT:$<CONFIG:Debug>>:/OPT:ICF>"
    # Remove unused functions and data, in particular, WinRT types
    "$<$<NOT:$<CONFIG:Debug>>:/OPT:REF>"
  )
  # Statically link the *C++* runtime, but dynamically link the UCRT
  # This gets us most of the benefit of statically linking, but without most of the size costs
  #
  # This requires Windows 10 - or, if you're running Windows 7, requires up-to-date
  # Windows updates, so maybe you don't want this
  #
  # Saves ~ 500kb
  option(ENABLE_HYBRID_CRT "Dynamically link the UCRT (raises OS update requirements)" ON)
  if (ENABLE_HYBRID_CRT)
    add_link_options(
      "/DEFAULTLIB:ucrt$<$<CONFIG:Debug>:d>.lib"
      "/NODEFAULTLIB:libucrt$<$<CONFIG:Debug>:d>.lib"
    )
  endif ()
endif()

add_subdirectory(dll)
add_subdirectory(src)
