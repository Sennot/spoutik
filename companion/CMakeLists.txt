cmake_minimum_required(VERSION 3.21)
project(LayoutCompanion VERSION 0.4.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config} "${CMAKE_BINARY_DIR}/bin")
endforeach()

include(FetchContent)
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.10
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(SDL3)

find_package(OpenGL REQUIRED)
add_executable(LayoutCompanion main.cpp)
target_include_directories(LayoutCompanion PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_compile_definitions(LayoutCompanion PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(LayoutCompanion PRIVATE SDL3::SDL3 OpenGL::GL)

if(MSVC)
    target_compile_options(LayoutCompanion PRIVATE /W4 /permissive-)
else()
    target_compile_options(LayoutCompanion PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_custom_command(TARGET LayoutCompanion POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:SDL3-shared>"
        "$<TARGET_FILE_DIR:LayoutCompanion>"
    VERBATIM
)
