cmake_minimum_required(VERSION 3.21)
project(SpoutLayoutCapture VERSION 0.3.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "GEODE_SDK is not set. Install Geode SDK or use the included GitHub Actions workflow.")
endif()

set(XDBOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/xdbot")
set(SPOUT_DLL "${CMAKE_CURRENT_SOURCE_DIR}/resources/spout/SpoutLibrary.dll")
set(SPOUT_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/vendor/spout/SpoutLibrary.h")

if (NOT EXISTS "${XDBOT_DIR}/layout_mode.hpp" OR NOT EXISTS "${XDBOT_DIR}/layout_mode.cpp")
    message(FATAL_ERROR "XDBot Layout Mode source is missing. Run: python tools/bootstrap_deps.py")
endif()
if (WIN32 AND (NOT EXISTS "${SPOUT_DLL}" OR NOT EXISTS "${SPOUT_HEADER}"))
    message(FATAL_ERROR "SpoutLibrary.dll/header is missing. Run: python tools/bootstrap_deps.py")
endif()

add_library(${PROJECT_NAME} SHARED
    src/main.cpp
    src/FrameCompositor.cpp
    src/LayoutMirror.cpp
    src/SpoutSender.cpp
    vendor/xdbot/layout_mode.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/xdbot"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/spout"
)

if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(${PROJECT_NAME} opengl32)
endif()

add_subdirectory("$ENV{GEODE_SDK}" "${CMAKE_CURRENT_BINARY_DIR}/geode")
setup_geode_mod(${PROJECT_NAME})
