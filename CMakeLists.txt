cmake_minimum_required(VERSION 3.20)
project(OpenCapture VERSION 0.1.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
  message(FATAL_ERROR "OpenCapture is a Windows application.")
endif()

option(OC_BUILD_TESTS "Build unit tests" ON)

configure_file(src/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/version.h @ONLY)

# Shared compile settings ------------------------------------------------------
add_library(oc_options INTERFACE)
target_compile_definitions(oc_options INTERFACE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
                                                _WIN32_WINNT=0x0A00 WINVER=0x0A00)
target_include_directories(oc_options INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/generated)
if(MSVC)
  target_compile_options(oc_options INTERFACE /W4 /utf-8 /permissive-)
else()
  target_compile_options(oc_options INTERFACE -Wall -Wextra -Wno-missing-field-initializers)
endif()

# Pure logic (no Win32), unit tested --------------------------------------------
add_library(oc_logic STATIC src/region_logic.cpp)
target_link_libraries(oc_logic PUBLIC oc_options)

# Application ------------------------------------------------------------------
add_executable(OpenCapture WIN32
  src/main.cpp
  src/app_icon.cpp
  src/image_io.cpp
  src/region_overlay.cpp
  src/surface.cpp
  src/viewer.cpp
  src/resources.rc
)
target_link_libraries(OpenCapture PRIVATE oc_logic oc_options
  user32 gdi32 shell32 ole32 oleaut32 uuid windowscodecs comdlg32 shcore)

if(MSVC)
  # The manifest is embedded through resources.rc.
  target_link_options(OpenCapture PRIVATE /MANIFEST:NO)
  set_property(TARGET OpenCapture oc_logic PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
  # MinGW / llvm-mingw: wWinMain entry point and a self-contained executable.
  target_link_options(OpenCapture PRIVATE -municode -static)
endif()

# Tests ------------------------------------------------------------------------
if(OC_BUILD_TESTS)
  enable_testing()
  add_executable(test_region_logic tests/test_region_logic.cpp)
  target_link_libraries(test_region_logic PRIVATE oc_logic)
  if(MSVC)
    set_property(TARGET test_region_logic PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  else()
    target_link_options(test_region_logic PRIVATE -static)
  endif()
  add_test(NAME region_logic COMMAND test_region_logic)
endif()
