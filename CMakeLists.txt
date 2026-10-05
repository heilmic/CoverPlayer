cmake_minimum_required(VERSION 3.18)

project(CoverPlayer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(COVERPLAYER_BUILD_TESTS "Build CoverPlayer tests" ON)
option(COVERPLAYER_BUNDLE_WINDOWS_RUNTIME "Copy MinGW runtime DLLs beside the executable" ON)

include(CTest)

find_package(PkgConfig REQUIRED)
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
    add_executable(coverplayer_sdl_input_test tests/sdl_input_test.cpp src/platform/sdl/sdl_input.cpp)
    target_include_directories(coverplayer_sdl_input_test PRIVATE include src)
    target_link_libraries(coverplayer_sdl_input_test PRIVATE PkgConfig::SDL2)
    add_test(NAME sdl_input COMMAND coverplayer_sdl_input_test)
    add_executable(coverplayer_player_seek_test
        tests/mpg123_player_seek_test.cpp
        src/audio/mpg123_sdl_player.cpp
        src/audio/mp3_duration.cpp
    )
    target_include_directories(coverplayer_player_seek_test PRIVATE include src)
    target_link_libraries(coverplayer_player_seek_test PRIVATE PkgConfig::SDL2 PkgConfig::MPG123)
    add_executable(coverplayer_screenshot_tool
        tests/screenshot_tool.cpp
        src/platform/sdl/sdl_renderer.cpp
    )
    target_include_directories(coverplayer_screenshot_tool PRIVATE include src)
    target_link_libraries(coverplayer_screenshot_tool PRIVATE PkgConfig::SDL2 PkgConfig::SDL2_TTF PkgConfig::SDL2_IMAGE)
endif()

add_executable(coverplayer
    src/main.cpp
    src/audio/mpg123_sdl_player.cpp
    src/persistence/file_progress_store.cpp
    src/persistence/file_library_cache.cpp
    src/platform/sdl/sdl_platform.cpp
    src/platform/sdl/sdl_renderer.cpp
    src/platform/sdl/sdl_input.cpp
    src/platform/sdl/linux_audio_system.cpp
    src/platform/sdl/linux_background_session.cpp
    src/platform/sdl/linux_battery.cpp
    src/platform/native_file_system.cpp
)
target_include_directories(coverplayer PRIVATE include src)
if(MSVC)
    target_compile_options(coverplayer PRIVATE /W4 /permissive-)
else()
    target_compile_options(coverplayer PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(coverplayer PRIVATE
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
