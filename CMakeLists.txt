cmake_minimum_required(VERSION 3.15)

# iOS (platform/ios/README.md): the oldest iOS version the app runs on, unless given
if(CMAKE_SYSTEM_NAME STREQUAL "iOS" AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "Oldest iOS version to run on")
endif()

project(Optica LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Source layout:
#   core/               portable: the model, the editing rules (controller), the layout
#                       and the viewport, and their unit tests. Built on every platform.
#   platform/windows/   the Win32 + GDI front end (WinMain, window, renderer).
#   platform/sdl/       the SDL3 + Dear ImGui front end (Linux, Android).
#   android/            the Android Studio / Gradle project; it builds this CMake
#                       project with OPTICA_FRONTEND=SDL for each Android ABI.
#   platform/ios/       the iOS app's Info.plist and icon; CMake's Xcode generator
#                       builds the app (platform/ios/README.md).

# Which front end to build:
#   GDI   the Windows program (Windows only; the default there)
#   SDL   the SDL3 + Dear ImGui program (the default elsewhere; downloads SDL3 and
#         Dear ImGui, see platform/sdl/CMakeLists.txt)
#   NONE  just the core and its tests
if(WIN32)
    set(OPTICA_FRONTEND_DEFAULT GDI)
else()
    set(OPTICA_FRONTEND_DEFAULT SDL)
endif()
set(OPTICA_FRONTEND ${OPTICA_FRONTEND_DEFAULT} CACHE STRING "Front end to build: GDI, SDL or NONE")
set_property(CACHE OPTICA_FRONTEND PROPERTY STRINGS GDI SDL NONE)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---- Core: no platform code ------------------------------------------------
add_library(optica_core STATIC
    core/layout.h
    core/model/scene.h
    core/model/optics.h
    core/model/ray_tracer.h
    core/model/ray_tracer.cpp
    core/model/imaging.h
    core/model/imaging.cpp
    core/model/scene_file.h
    core/model/scene_file.cpp
    core/controller/editor_view.h
    core/controller/editor_controller.h
    core/controller/editor_controller.cpp
    core/view/viewport.h
)
# Front ends include core headers as "layout.h", "model/scene.h", "view/viewport.h", ...
target_include_directories(optica_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/core)

if(MSVC)
    target_compile_options(optica_core PRIVATE /W3 /Zc:__cplusplus)
    target_compile_definitions(optica_core PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(optica_core PRIVATE -Wall)
endif()

# ---- Unit tests (portable: no Windows needed; not built for Android or iOS) -
enable_testing()
if(NOT ANDROID AND NOT IOS)
    foreach(test scene_test optics_test controller_test viewport_test ray_tracer_test imaging_test)
        add_executable(${test} core/tests/${test}.cpp)
        target_link_libraries(${test} PRIVATE optica_core)
        add_test(NAME ${test} COMMAND ${test})
    endforeach()
endif()

# ---- Windows front end: Win32 + GDI ----------------------------------------
if(OPTICA_FRONTEND STREQUAL "GDI")
    if(NOT WIN32)
        message(FATAL_ERROR "OPTICA_FRONTEND=GDI needs Windows.")
    endif()

    # WIN32 makes this a GUI app (WinMain entry point, no console window).
    add_executable(optica WIN32
        platform/windows/main.cpp
        platform/windows/view/gdi_renderer.h
        platform/windows/view/gdi_renderer.cpp
        platform/windows/view/gdi_editor_view.h
        platform/windows/view/gdi_editor_view.cpp
        platform/windows/view/main_window.h
        platform/windows/view/main_window.cpp
    )
    target_link_libraries(optica PRIVATE optica_core)

    # Uses the ANSI Win32 API (char class name, LPSTR WinMain), so UNICODE is not defined.
    target_compile_definitions(optica PRIVATE WIN32_LEAN_AND_MEAN)

    # user32: windows, buttons, cursors, message boxes; gdi32: pens, brushes, BitBlt.
    target_link_libraries(optica PRIVATE user32 gdi32)

    if(MSVC)
        # /Zc:__cplusplus makes __cplusplus report the real language version.
        target_compile_options(optica PRIVATE /W3 /Zc:__cplusplus)
        target_compile_definitions(optica PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(optica PRIVATE -Wall)
    endif()

    # Copy datos.txt next to the executable if it exists.
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/datos.txt)
        add_custom_command(TARGET optica POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${CMAKE_CURRENT_SOURCE_DIR}/datos.txt
                    $<TARGET_FILE_DIR:optica>/datos.txt)
    endif()
elseif(OPTICA_FRONTEND STREQUAL "SDL")
    add_subdirectory(platform/sdl)
elseif(OPTICA_FRONTEND STREQUAL "NONE")
    message(STATUS "No front end selected: only the core and its unit tests are built.")
else()
    message(FATAL_ERROR "Unknown OPTICA_FRONTEND '${OPTICA_FRONTEND}': use GDI, SDL or NONE.")
endif()
