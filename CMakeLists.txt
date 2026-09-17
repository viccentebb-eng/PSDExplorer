cmake_minimum_required(VERSION 3.20)
project(PSDExplorer VERSION 1.1.2 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  # All source files are UTF-8. This prevents mojibake such as "CachÃ©" / "DiagnÃ³stico"
  # on Windows installations whose active code page is not UTF-8.
  add_compile_options(/utf-8)
  # Ship without requiring a separate VC++ runtime installation.
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_library(psdcore STATIC src/PsdCore.cpp)
target_include_directories(psdcore PUBLIC src)

option(PSD_EXPLORER_BUILD_TESTS "Build core tests" ON)
if(PSD_EXPLORER_BUILD_TESTS)
  enable_testing()
  add_executable(psdcore_tests tests/test_psd_core.cpp)
  target_link_libraries(psdcore_tests PRIVATE psdcore)
  add_test(NAME psdcore_tests COMMAND psdcore_tests)
endif()

if(WIN32)
  add_library(PSDExplorerShell SHARED
    src/dllmain.cpp
    src/ClassFactory.cpp
    src/ThumbnailProvider.cpp
    src/PreviewHandler.cpp
    src/WicUtil.cpp
    src/ComStreamAdapter.cpp
    src/PSDExplorerShell.def
  )
  target_include_directories(PSDExplorerShell PRIVATE src)
  target_compile_definitions(PSDExplorerShell PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(PSDExplorerShell PRIVATE psdcore windowscodecs ole32 shell32 shlwapi comctl32 gdi32 user32)
  set_target_properties(PSDExplorerShell PROPERTIES OUTPUT_NAME "PSDExplorerShell")

  add_executable(PSDExplorerSetup WIN32 manager/main.cpp)
  target_include_directories(PSDExplorerSetup PRIVATE src)
  target_compile_definitions(PSDExplorerSetup PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(PSDExplorerSetup PRIVATE psdcore shell32 shlwapi advapi32 comctl32 user32 gdi32 ole32)

  # Small elevated helper. Explorer's isolated thumbnail host can fail to see a
  # per-user InprocServer32 registration on some Windows 11 configurations.
  # The helper copies only the Shell DLL to Program Files and registers the CLSIDs
  # at machine scope after the user accepts the normal UAC prompt.
  add_executable(PSDExplorerAdmin WIN32 manager/admin.cpp)
  target_include_directories(PSDExplorerAdmin PRIVATE src)
  target_compile_definitions(PSDExplorerAdmin PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(PSDExplorerAdmin PRIVATE shell32 advapi32 user32)
endif()
