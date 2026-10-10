cmake_minimum_required(VERSION 3.18)

project(CoverPlayer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(COVERPLAYER_BUILD_TESTS "Build CoverPlayer tests" ON)
option(COVERPLAYER_BUNDLE_WINDOWS_RUNTIME "Copy MinGW runtime DLLs beside the executable" ON)
option(COVERPLAYER_BUILD_SWITCH_PREVIEW "Build the desktop Switch-layout screenshot tool" OFF)

include(CTest)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
if(NINTENDO_SWITCH)
    # Switch libraries are static; include their private link dependencies.
    set(PKG_CONFIG_ARGN --static)
    set(COVERPLAYER_BUILD_TESTS OFF)
endif()
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(SDL2_TTF REQUIRED IMPORTED_TARGET SDL2_ttf)
pkg_check_modules(SDL2_IMAGE REQUIRED IMPORTED_TARGET SDL2_image)
pkg_check_modules(MPG123 REQUIRED IMPORTED_TARGET libmpg123)

add_library(coverplayer_core
    src/app/application.cpp
    src/library/library_scanner.cpp
    src/audio/mp3_duration.cpp
)
target_include_directories(coverplayer_core PUBLIC include)
target_include_directories(coverplayer_core PRIVATE src)
target_compile_features(coverplayer_core PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(coverplayer_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(coverplayer_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING AND COVERPLAYER_BUILD_TESTS)
    add_executable(coverplayer_library_test tests/library_scanner_test.cpp)
    target_link_libraries(coverplayer_library_test PRIVATE coverplayer_core)
    target_include_directories(coverplayer_library_test PRIVATE include)
    add_test(NAME library_scanner COMMAND coverplayer_library_test)
    add_executable(coverplayer_application_test tests/application_navigation_test.cpp)
    target_link_libraries(coverplayer_application_test PRIVATE coverplayer_core)
    target_include_directories(coverplayer_application_test PRIVATE include)
    add_test(NAME application_navigation COMMAND coverplayer_application_test)
    add_executable(coverplayer_duration_test tests/mp3_duration_test.cpp src/audio/mp3_duration.cpp)
    target_include_directories(coverplayer_duration_test PRIVATE include src)
    add_test(NAME mp3_duration COMMAND coverplayer_duration_test)
    add_executable(coverplayer_native_file_system_test tests/native_file_system_test.cpp src/platform/native_file_system.cpp)
    target_include_directories(coverplayer_native_file_system_test PRIVATE include src)
    target_link_libraries(coverplayer_native_file_system_test PRIVATE PkgConfig::SDL2)
    add_test(NAME native_file_system COMMAND coverplayer_native_file_system_test)
    add_executable(coverplayer_library_cache_test tests/library_cache_test.cpp src/persistence/file_library_cache.cpp)
    target_include_directories(coverplayer_library_cache_test PRIVATE include src)
    target_link_libraries(coverplayer_library_cache_test PRIVATE PkgConfig::SDL2)
    add_test(NAME library_cache COMMAND coverplayer_library_cache_test)
    add_executable(coverplayer_progress_store_test tests/file_progress_store_test.cpp src/persistence/file_progress_store.cpp)
    target_include_directories(coverplayer_progress_store_test PRIVATE include src)
    target_link_libraries(coverplayer_progress_store_test PRIVATE PkgConfig::SDL2)
    add_test(NAME progress_store COMMAND coverplayer_progress_store_test)
    add_executable(coverplayer_cover_flow_motion_test tests/cover_flow_motion_test.cpp)
    target_include_directories(coverplayer_cover_flow_motion_test PRIVATE src)
    add_test(NAME cover_flow_motion COMMAND coverplayer_cover_flow_motion_test)
    add_executable(coverplayer_renderer_cache_test tests/renderer_cover_cache_test.cpp src/platform/sdl/sdl_renderer.cpp)
    target_include_directories(coverplayer_renderer_cache_test PRIVATE include src)
    target_link_libraries(coverplayer_renderer_cache_test PRIVATE Threads::Threads PkgConfig::SDL2 PkgConfig::SDL2_IMAGE PkgConfig::SDL2_TTF)
    add_test(NAME renderer_cover_cache COMMAND coverplayer_renderer_cache_test)
    add_executable(coverplayer_sdl_input_test tests/sdl_input_test.cpp src/platform/sdl/sdl_input.cpp)
    target_include_directories(coverplayer_sdl_input_test PRIVATE include src)
    target_link_libraries(coverplayer_sdl_input_test PRIVATE PkgConfig::SDL2)
    add_test(NAME sdl_input COMMAND coverplayer_sdl_input_test)
    add_executable(coverplayer_background_session_test
        tests/linux_background_session_test.cpp
        src/platform/sdl/audio_ducker.cpp
        src/platform/sdl/linux_background_session.cpp
    )
    target_include_directories(coverplayer_background_session_test PRIVATE include src)
    target_link_libraries(coverplayer_background_session_test PRIVATE PkgConfig::SDL2)
    add_test(NAME background_session COMMAND coverplayer_background_session_test)
    add_executable(coverplayer_audio_ducker_test
        tests/audio_ducker_test.cpp
        src/platform/sdl/audio_ducker.cpp
        src/platform/sdl/linux_background_session.cpp
    )
    target_include_directories(coverplayer_audio_ducker_test PRIVATE include src)
    target_link_libraries(coverplayer_audio_ducker_test PRIVATE PkgConfig::SDL2)
    add_test(NAME audio_ducker COMMAND coverplayer_audio_ducker_test)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT CMAKE_CROSSCOMPILING)
        find_package(Python3 COMPONENTS Interpreter QUIET)
        if(Python3_Interpreter_FOUND)
            add_test(NAME audio_ducker_lifecycle
                COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/audio_ducker_integration_test.py"
                    $<TARGET_FILE:coverplayer_audio_ducker_test>)
        endif()
    endif()
    add_executable(coverplayer_player_seek_test
        tests/mpg123_player_seek_test.cpp
        src/audio/mpg123_sdl_player.cpp
        src/audio/mp3_duration.cpp
    )
    target_include_directories(coverplayer_player_seek_test PRIVATE include src)
    target_link_libraries(coverplayer_player_seek_test PRIVATE PkgConfig::SDL2 PkgConfig::MPG123)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_executable(coverplayer_playback_power_test tests/playback_power_test.cpp src/platform/sdl/playback_power.cpp)
        target_include_directories(coverplayer_playback_power_test PRIVATE include src)
        target_link_libraries(coverplayer_playback_power_test PRIVATE PkgConfig::SDL2)
        add_test(NAME playback_power COMMAND coverplayer_playback_power_test)
    endif()
    add_executable(coverplayer_screenshot_tool
        tests/screenshot_tool.cpp
        src/platform/sdl/sdl_renderer.cpp
    )
    target_include_directories(coverplayer_screenshot_tool PRIVATE include src)
    target_link_libraries(coverplayer_screenshot_tool PRIVATE Threads::Threads PkgConfig::SDL2 PkgConfig::SDL2_TTF PkgConfig::SDL2_IMAGE)
endif()

if(NINTENDO_SWITCH)
    set(COVERPLAYER_PLATFORM_SOURCES
        src/platform/switch/switch_main.cpp
        src/platform/switch/switch_platform.cpp
        src/platform/switch/switch_renderer.cpp)
else()
    set(COVERPLAYER_PLATFORM_SOURCES
        src/main.cpp
        src/platform/sdl/playback_power.cpp
        src/platform/sdl/sdl_platform.cpp
        src/platform/sdl/linux_audio_system.cpp
        src/platform/sdl/audio_ducker.cpp
        src/platform/sdl/linux_background_session.cpp
        src/platform/sdl/linux_battery.cpp)
endif()

add_executable(coverplayer
    ${COVERPLAYER_PLATFORM_SOURCES}
    src/audio/mpg123_sdl_player.cpp
    src/persistence/file_progress_store.cpp
    src/persistence/file_library_cache.cpp
    src/platform/sdl/sdl_renderer.cpp
    src/platform/sdl/sdl_input.cpp
    src/platform/native_file_system.cpp
)
target_include_directories(coverplayer PRIVATE include src)
if(NINTENDO_SWITCH)
    target_compile_definitions(coverplayer PRIVATE COVERPLAYER_SWITCH_UI=1)
endif()
if(MSVC)
    target_compile_options(coverplayer PRIVATE /W4 /permissive-)
else()
    target_compile_options(coverplayer PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(coverplayer PRIVATE
    Threads::Threads
    coverplayer_core
    PkgConfig::SDL2
    PkgConfig::SDL2_TTF
    PkgConfig::SDL2_IMAGE
    PkgConfig::MPG123
)

add_custom_command(TARGET coverplayer POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:coverplayer>/assets/fonts"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/RobotoMono-Bold.ttf"
        "$<TARGET_FILE_DIR:coverplayer>/assets/fonts/RobotoMono-Bold.ttf"
    COMMENT "Copying CoverPlayer assets"
)

install(TARGETS coverplayer RUNTIME DESTINATION bin)

if(COVERPLAYER_BUILD_SWITCH_PREVIEW AND NOT NINTENDO_SWITCH)
    add_executable(coverplayer_switch_preview tests/screenshot_tool.cpp
        src/platform/sdl/sdl_renderer.cpp src/platform/switch/switch_renderer.cpp)
    target_include_directories(coverplayer_switch_preview PRIVATE include src)
    target_compile_definitions(coverplayer_switch_preview PRIVATE COVERPLAYER_SWITCH_UI=1)
    target_link_libraries(coverplayer_switch_preview PRIVATE Threads::Threads
        PkgConfig::SDL2 PkgConfig::SDL2_TTF PkgConfig::SDL2_IMAGE)
endif()

if(NINTENDO_SWITCH)
    set(COVERPLAYER_ROMFS "${CMAKE_CURRENT_BINARY_DIR}/romfs")
    file(MAKE_DIRECTORY "${COVERPLAYER_ROMFS}/assets/fonts")
    configure_file(assets/fonts/RobotoMono-Bold.ttf
        "${COVERPLAYER_ROMFS}/assets/fonts/RobotoMono-Bold.ttf" COPYONLY)
    nx_generate_nacp(OUTPUT CoverPlayer.nacp NAME "CoverPlayer"
        AUTHOR "heilmic" VERSION "${PROJECT_VERSION}-sw-a5")
    nx_create_nro(coverplayer OUTPUT CoverPlayer.nro
        NACP "${CMAKE_CURRENT_BINARY_DIR}/CoverPlayer.nacp"
        ICON "${CMAKE_CURRENT_SOURCE_DIR}/packaging/switch/icon.jpg"
        ROMFS "${COVERPLAYER_ROMFS}")
endif()

if(WIN32 AND COVERPLAYER_BUNDLE_WINDOWS_RUNTIME)
    get_filename_component(COVERPLAYER_RUNTIME_DIRECTORY "${CMAKE_CXX_COMPILER}" DIRECTORY)
    add_custom_command(TARGET coverplayer POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
            -DEXECUTABLE=$<TARGET_FILE:coverplayer>
            -DDESTINATION=$<TARGET_FILE_DIR:coverplayer>
            -DRUNTIME_DIRECTORY=${COVERPLAYER_RUNTIME_DIRECTORY}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_runtime_dependencies.cmake"
        COMMENT "Copying Windows runtime dependencies"
        VERBATIM
    )
endif()
