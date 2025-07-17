find_package(fmt CONFIG REQUIRED)
find_package(unofficial-hash-library CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(libzip CONFIG REQUIRED)
find_package(restclient-cpp CONFIG REQUIRED)
find_package(directxtk CONFIG REQUIRED)

# vcpkg port of neflib does not define a target
find_library(NEFLIB_PATH NAMES neflib REQUIRED)

# Header files don't *need* to be specified, but it helps some IDEs
add_executable(
  Updater
  WIN32
  Crypto.cpp Crypto.h
  InstanceConfig.Dialogs.cpp
  InstanceConfig.Download.cpp
  InstanceConfig.Postpone.cpp
  InstanceConfig.Setup.cpp
  InstanceConfig.TaskScheduler.cpp
  InstanceConfig.Template.cpp
  InstanceConfig.Updater.cpp
  InstanceConfig.Web.cpp
  InstanceConfig.cpp
  MimeTypes.cpp MimeTypes.h
  NAuthenticode.cpp NAuthenticode.h
  UpdateRelease.cpp
  imgui_md.cpp imgui_md.h
  main.cpp
  markdown.cpp
  md4c.c md4c.h
  models/CommonTypes.hpp
  models/DownloadLocationConfig.h
  models/InstanceConfig.hpp
  models/MergedConfig.hpp
  models/ProductDetection.hpp
  models/SharedConfig.hpp
  models/SignatureValidation.hpp
  models/UpdateConfig.hpp
  models/UpdateRelease.hpp
  models/UpdateResponse.hpp
  pch.cpp pch.h
  ui.cpp UI.h
  util.cpp
)
target_include_directories(
  Updater
  PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/models"
)
target_link_libraries(
  Updater
  PRIVATE
  # vcpkg
  imgui::imgui
  fmt::fmt
  libzip::zip
  restclient-cpp
  unofficial::hash-library
  Microsoft::DirectXTK
  "${NEFLIB_PATH}"
  # system
  Comctl32
  crypt32
  D3D11
  dwmapi
  Propsys
  Rpcrt4
  Shlwapi
  taskschd
  Version
)

# We *should* be able to directly add vicius.rc to the executable sources,
# and CMake *should* build self-updater first when it sees it's a dependency
# via the generator expression... but that's not working. It seems like the
# RC file compilation in cmake is a build step that is executed before any
# dependencies. So, add a dummy step to force the ordering.
set(RC_FILE_COPY "${CMAKE_CURRENT_BINARY_DIR}/vicius-copy.rc")
add_custom_command(
  OUTPUT "${RC_FILE_COPY}"
  COMMAND
  "${CMAKE_COMMAND}"
    -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/vīcĭus.rc"
    "${RC_FILE_COPY}"
  DEPENDS
  self-updater
)
target_sources(Updater PRIVATE "${RC_FILE_COPY}")
target_compile_definitions(
  Updater
  PRIVATE
  "SELF_UPDATER_PATH=\"$<TARGET_FILE:self-updater>\""
)
target_include_directories(
  Updater
  PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
)
install(TARGETS Updater)