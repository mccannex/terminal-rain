cmake_minimum_required(VERSION 3.16)
project(TerminalRain VERSION 1.1.0 LANGUAGES CXX)

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

# Warning flags applied to our own targets only -- never to vendored SDL2,
# which we don't want to be responsible for the warnings of.
if(MSVC)
    set(TERMINAL_RAIN_WARNINGS /W4)
else()
    set(TERMINAL_RAIN_WARNINGS -Wall -Wextra)
endif()

add_library(terminal_rain_core STATIC
    core/glyph_atlas.cpp
    core/glyph_atlas_data.cpp
    core/stream_field.cpp
    core/app_loop.cpp)
target_link_libraries(terminal_rain_core PUBLIC SDL2::SDL2-static)
target_include_directories(terminal_rain_core PUBLIC ${CMAKE_SOURCE_DIR})
target_compile_options(terminal_rain_core PRIVATE ${TERMINAL_RAIN_WARNINGS})

# Opt-in rendering/lifecycle regression checks; not part of release artifacts.
option(TERMINAL_RAIN_BUILD_TESTS "Build rendering regression checks" OFF)
if(TERMINAL_RAIN_BUILD_TESTS)
    enable_testing()
    add_executable(terminal_rain_zombie_tests tests/zombie_confirmation_tests.cpp)
    target_include_directories(terminal_rain_zombie_tests PRIVATE ${CMAKE_SOURCE_DIR})
    target_compile_options(terminal_rain_zombie_tests PRIVATE ${TERMINAL_RAIN_WARNINGS})
    add_test(NAME zombie_confirmation COMMAND terminal_rain_zombie_tests)
    add_executable(terminal_rain_rendering_tests tests/rendering_tests.cpp)
    target_link_libraries(terminal_rain_rendering_tests PRIVATE terminal_rain_core SDL2::SDL2-static)
    target_compile_options(terminal_rain_rendering_tests PRIVATE ${TERMINAL_RAIN_WARNINGS})
    # GNU ld wrapping injects SDL failures and verifies texture ownership
    # without adding test hooks to production code. Other toolchains still
    # run the portable rendering/reset checks.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT APPLE)
        target_compile_definitions(terminal_rain_rendering_tests PRIVATE TERMINAL_RAIN_TEST_FAULTS)
        target_link_options(terminal_rain_rendering_tests PRIVATE
            -Wl,--wrap=SDL_CreateTexture -Wl,--wrap=SDL_SetRenderTarget
            -Wl,--wrap=SDL_DestroyTexture -Wl,--wrap=SDL_DestroyRenderer)
    endif()
    add_test(NAME rendering COMMAND terminal_rain_rendering_tests)
    if(APPLE)
        add_executable(terminal_rain_color_tests tests/macos_color_tests.mm)
        target_link_libraries(terminal_rain_color_tests PRIVATE terminal_rain_core
            "-framework Cocoa" "-framework QuartzCore" "-framework CoreGraphics")
        target_compile_options(terminal_rain_color_tests PRIVATE -fobjc-arc ${TERMINAL_RAIN_WARNINGS})
        add_test(NAME color_macos COMMAND terminal_rain_color_tests)
        set_tests_properties(color_macos PROPERTIES LABELS native)
    endif()
    if(WIN32)
        add_test(NAME rendering_windows COMMAND terminal_rain_rendering_tests --native)
        set_tests_properties(rendering_windows PROPERTIES LABELS native)
    endif()
endif()

# Offline frame capture for the README's animated preview (see
# tools/make_preview.py). Off by default; not part of any release.
option(TERMINAL_RAIN_BUILD_TOOLS "Build the preview capture tool" OFF)
if(TERMINAL_RAIN_BUILD_TOOLS)
    add_executable(terminal_rain_capture tools/capture/main.cpp)
    target_link_libraries(terminal_rain_capture PRIVATE terminal_rain_core SDL2::SDL2-static SDL2::SDL2main)
    target_compile_options(terminal_rain_capture PRIVATE ${TERMINAL_RAIN_WARNINGS})
endif()

if(UNIX AND NOT APPLE)
    # Plain fullscreen executable, launched by KDE's powerdevil idle hook --
    # Linux has no .scr/.saver-style screensaver plugin convention. See
    # platform/linux/AGENT_CONTEXT.md.
    add_executable(terminal_rain platform/linux/main.cpp)
    target_link_libraries(terminal_rain PRIVATE terminal_rain_core SDL2::SDL2-static SDL2::SDL2main)
    target_compile_options(terminal_rain PRIVATE ${TERMINAL_RAIN_WARNINGS})
    set_target_properties(terminal_rain PROPERTIES OUTPUT_NAME "terminal-rain")
endif()

if(WIN32)
    add_executable(terminal_rain WIN32 platform/windows/main.cpp)
    target_link_libraries(terminal_rain PRIVATE terminal_rain_core SDL2::SDL2-static SDL2::SDL2main)
    target_compile_options(terminal_rain PRIVATE ${TERMINAL_RAIN_WARNINGS})

    # A .scr is just a renamed .exe -- Windows recognizes the extension and
    # adds Install/Test/Open to its right-click menu with no additional
    # registration needed beyond the file existing. With SDL2 statically
    # linked and the glyph atlas embedded (core/glyph_atlas_data.cpp), this is
    # the entire deliverable -- no companion files needed alongside it.
    # Windows' screensaver picker derives its displayed name straight from
    # the filename (no version resource is read), so the file is named
    # "Terminal Rain.scr" -- with the space -- rather than the target's
    # underscored build name, and distinctly from the legacy build's
    # "Matrix Screensaver.scr" so the picker can't confuse the two.
    add_custom_command(TARGET terminal_rain POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:terminal_rain>
            "$<TARGET_FILE_DIR:terminal_rain>/Terminal Rain.scr")
endif()

if(APPLE)
    # A .saver is a loadable NSBundle plugin, not an app -- MODULE + BUNDLE
    # TRUE (rather than MACOSX_BUNDLE, which is for .app executables) is
    # CMake's way of producing that shape. legacyScreenSaver loads it via
    # NSPrincipalClass (TerminalRainView, see platform/macos/Info.plist),
    # instantiating one view per display itself -- no Windows/Linux-style
    # multi-monitor loop or event-pump/exit-on-input logic needed here, see
    # platform/macos/TerminalRainView.mm.
    #
    # thumbnail.png/@2x.png (generated by tools/generate_macos_thumbnail.py)
    # are the picker grid's tile image -- unlike the larger "selected"
    # preview canvas, that grid never instantiates TerminalRainView at all, it
    # just displays these two loose Resources files if present (90x58 @1x /
    # 180x116 @2x, the convention other .saver bundles use), else falls back
    # to a generic icon. No Info.plist entry needed for this.
    set(macos_saver_thumbnails
        platform/macos/thumbnail.png
        platform/macos/thumbnail@2x.png)
    set_source_files_properties(${macos_saver_thumbnails} PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources)

    add_library(terminal_rain_saver MODULE platform/macos/TerminalRainView.mm ${macos_saver_thumbnails})
    target_compile_options(terminal_rain_saver PRIVATE -fobjc-arc ${TERMINAL_RAIN_WARNINGS})
    target_link_libraries(terminal_rain_saver PRIVATE
        terminal_rain_core SDL2::SDL2-static "-framework Cocoa" "-framework ScreenSaver"
        "-framework CoreGraphics")
    set_target_properties(terminal_rain_saver PROPERTIES
        OUTPUT_NAME "Terminal Rain"
        BUNDLE TRUE
        BUNDLE_EXTENSION "saver"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/platform/macos/Info.plist")

    # Ad-hoc signed (no Apple Developer identity involved) so Gatekeeper/AMFI
    # accept a locally built bundle for local install/testing via
    # ~/Library/Screen Savers -- see platform/macos/AGENT_CONTEXT.md for what
    # a real notarized distribution build would need on top of this.
    add_custom_command(TARGET terminal_rain_saver POST_BUILD
        COMMAND codesign --force --deep --sign - $<TARGET_BUNDLE_DIR:terminal_rain_saver>)
    # POST_BUILD only runs when the module relinks. Without this, changing
    # only a thumbnail recopies it into the already-signed bundle and breaks
    # the signature ("a sealed resource is missing or invalid").
    set_property(TARGET terminal_rain_saver APPEND PROPERTY
        LINK_DEPENDS ${CMAKE_SOURCE_DIR}/platform/macos/thumbnail.png
                     ${CMAKE_SOURCE_DIR}/platform/macos/thumbnail@2x.png)
endif()
