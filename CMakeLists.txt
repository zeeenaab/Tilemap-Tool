cmake_minimum_required(VERSION 3.16)
project(TilemapTool LANGUAGES C CXX)   # SDL's sources are C, ours are C++

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to an optimized build for single-config generators (Ninja, Makefiles).
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# MSVC: link the C/C++ runtime statically so users don't need the VC++ redistributable.
# (Must be set before any targets are created, including SDL's.)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ---- SDL3: built from the vendored source in third_party/SDL, as a static library ----
set(SDL_SHARED       OFF CACHE BOOL "" FORCE)
set(SDL_STATIC       ON  CACHE BOOL "" FORCE)
set(SDL_TESTS        OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES     OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL      OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/SDL EXCLUDE_FROM_ALL)

# ---- The app ----
# WIN32 = GUI subsystem on Windows (no console window); ignored on other platforms.
add_executable(tilemap_tool WIN32
    src/main.cpp
    src/tileset.cpp
    src/tilemap.cpp
    src/ui.cpp
    src/resource.rc
)
set_target_properties(tilemap_tool PROPERTIES OUTPUT_NAME "TilemapTool")
target_include_directories(tilemap_tool PRIVATE third_party)   # for stb_image.h
target_link_libraries(tilemap_tool PRIVATE SDL3::SDL3)

# MinGW: fold libgcc/libstdc++/winpthread into the exe.
# Linux: at least fold in libgcc/libstdc++ (glibc and the windowing libs stay dynamic).
if(MINGW)
    target_link_options(tilemap_tool PRIVATE -static)
elseif(UNIX AND NOT APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(tilemap_tool PRIVATE -static-libgcc -static-libstdc++)
endif()
