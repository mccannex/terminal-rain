cmake_minimum_required(VERSION 3.16)
project(GlyphRain LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Statically linked so built binaries are self-contained (no SDL2.dll /
# libSDL2.so companion file needed alongside the executable).
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-2.30.9
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(SDL2)

add_library(glyph_rain_core STATIC
    core/glyph_atlas.cpp
    core/stream_field.cpp
    core/config.cpp
    core/app_loop.cpp)
target_link_libraries(glyph_rain_core PUBLIC SDL2::SDL2-static)
target_include_directories(glyph_rain_core PUBLIC ${CMAKE_SOURCE_DIR})

add_executable(glyph_rain_dev src/sdl_app/main.cpp)
target_link_libraries(glyph_rain_dev PRIVATE glyph_rain_core SDL2::SDL2-static SDL2::SDL2main)

# Only deploy the default config if the user doesn't already have one next
# to the exe -- rebuilds must never clobber a user's edited matrix.cfg.
# glyph_rain_dev (the dev/WSL sanity binary) keeps this for quick tuning
# iteration; the shipped glyph_rain screensaver deliberately doesn't (see
# below) and just uses its compiled-in defaults for now.
add_custom_command(TARGET glyph_rain_dev POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DSRC=${CMAKE_SOURCE_DIR}/config/matrix.default.cfg
        -DDEST=$<TARGET_FILE_DIR:glyph_rain_dev>/matrix.cfg
        -P ${CMAKE_SOURCE_DIR}/cmake/copy_if_missing.cmake)

if(WIN32)
    add_executable(glyph_rain WIN32 src/win32/main.cpp)
    target_link_libraries(glyph_rain PRIVATE glyph_rain_core SDL2::SDL2-static SDL2::SDL2main)

    # A .scr is just a renamed .exe -- Windows recognizes the extension and
    # adds Install/Test/Open to its right-click menu with no additional
    # registration needed beyond the file existing. With SDL2 statically
    # linked and the glyph atlas embedded (core/glyph_atlas_data.h), this is
    # the entire deliverable -- no companion files needed alongside it.
    # Windows' screensaver picker derives its displayed name straight from
    # the filename (no version resource is read), so the file is named
    # "Glyph Rain.scr" -- with the space -- rather than the target's
    # underscored build name, and distinctly from the legacy build's
    # "Matrix Screensaver.scr" so the picker can't confuse the two.
    add_custom_command(TARGET glyph_rain POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:glyph_rain>
            "$<TARGET_FILE_DIR:glyph_rain>/Glyph Rain.scr")
endif()
