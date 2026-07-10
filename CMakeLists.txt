cmake_minimum_required(VERSION 3.16)
project(MatrixSS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-2.30.9
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(SDL2)

add_library(matrix_core STATIC
    core/glyph_atlas.cpp
    core/stream_field.cpp
    core/config.cpp)
target_link_libraries(matrix_core PUBLIC SDL2::SDL2)
target_include_directories(matrix_core PUBLIC ${CMAKE_SOURCE_DIR})

add_executable(matrix_hello src/sdl_app/main.cpp)
target_link_libraries(matrix_hello PRIVATE matrix_core SDL2::SDL2 SDL2::SDL2main)

if(WIN32)
    add_custom_command(TARGET matrix_hello POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL2::SDL2>
            $<TARGET_FILE_DIR:matrix_hello>)
endif()

add_custom_command(TARGET matrix_hello POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/assets/fonts/cp437_vga_8x16.bmp
        $<TARGET_FILE_DIR:matrix_hello>)

# Only deploy the default config if the user doesn't already have one next
# to the exe -- rebuilds must never clobber a user's edited matrix.cfg.
add_custom_command(TARGET matrix_hello POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DSRC=${CMAKE_SOURCE_DIR}/config/matrix.default.cfg
        -DDEST=$<TARGET_FILE_DIR:matrix_hello>/matrix.cfg
        -P ${CMAKE_SOURCE_DIR}/cmake/copy_if_missing.cmake)
