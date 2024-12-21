find_package(argh CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(unofficial-curlpp CONFIG REQUIRED)

add_library(
  self-updater
  # "MODULE" is for a `SHARED` library that will be used via rundll32,
  # LoadLibrary+GetProcAddress etc, instead of being linked
  MODULE
  # Only build if explicitly asked for, or if something depends on it.
  # Doing this as a safety measure as vicius *should* depend on it
  EXCLUDE_FROM_ALL
  dll.cpp dll.h
  dllmain.cpp
  framework.h
)
target_link_libraries(
  self-updater
  PRIVATE
  argh
  fmt::fmt
  unofficial::curlpp::curlpp
)
target_compile_definitions(
  self-updater
  PRIVATE
  DLL_EXPORTS
)