cmake_minimum_required(VERSION 3.20)
project(GameBoyEmulator LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Static MSVC runtime so users don't need the VC++ redistributable.
# Must be set before any targets are created.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Default to Release for single-config generators (Ninja, Makefiles)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(SDL2 REQUIRED)

include(FetchContent)

# ---------- Dear ImGui ----------
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.0)

# ---------- tinyfiledialogs ----------
FetchContent_Declare(tinyfiledialogs
    GIT_REPOSITORY https://github.com/native-toolkit/libtinyfiledialogs.git
    GIT_TAG        master)   # pin to a commit hash for reproducible builds

FetchContent_MakeAvailable(imgui tinyfiledialogs)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer2.cpp)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL2::SDL2)

add_library(tinyfiledialogs STATIC
    ${tinyfiledialogs_SOURCE_DIR}/tinyfiledialogs.c)
target_include_directories(tinyfiledialogs PUBLIC
    ${tinyfiledialogs_SOURCE_DIR})
if(WIN32)
    target_link_libraries(tinyfiledialogs PUBLIC comdlg32 ole32)
endif()

# ---------- Emulator ----------
add_executable(gameboy
    src/main.cpp
    src/cpu.cpp
    src/mmu.cpp
    src/ppu.cpp
    src/apu.cpp)

# Hide the console window in Release builds (works with multi-config generators)
if(WIN32)
    set_target_properties(gameboy PROPERTIES
        WIN32_EXECUTABLE $<CONFIG:Release>)
endif()

# SDL2main must come before SDL2 (needed on MinGW, harmless elsewhere)
if(TARGET SDL2::SDL2main)
    target_link_libraries(gameboy PRIVATE SDL2::SDL2main)
endif()

target_link_libraries(gameboy PRIVATE
    SDL2::SDL2
    imgui
    tinyfiledialogs)

# Copy SDL2.dll next to the exe so it runs without SDL2 on the PATH
if(WIN32 AND TARGET SDL2::SDL2)
    add_custom_command(TARGET gameboy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:gameboy>)
endif()

install(TARGETS gameboy RUNTIME DESTINATION .)

if(WIN32)
    # The compiler lives in the same bin folder as SDL2.dll and the GCC runtime DLLs
    get_filename_component(TOOLCHAIN_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
    foreach(dll SDL2.dll libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
        if(EXISTS "${TOOLCHAIN_BIN}/${dll}")
            install(FILES "${TOOLCHAIN_BIN}/${dll}" DESTINATION .)
        endif()
    endforeach()
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/README.txt")
    install(FILES "${CMAKE_SOURCE_DIR}/README.txt" DESTINATION .)
endif()

install(TARGETS gameboy RUNTIME DESTINATION .)

if(WIN32)
    # The compiler lives in the same bin folder as SDL2.dll and the GCC runtime DLLs
    get_filename_component(TOOLCHAIN_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
    foreach(dll SDL2.dll libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
        if(EXISTS "${TOOLCHAIN_BIN}/${dll}")
            install(FILES "${TOOLCHAIN_BIN}/${dll}" DESTINATION .)
        endif()
    endforeach()
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/README.txt")
    install(FILES "${CMAKE_SOURCE_DIR}/README.txt" DESTINATION .)
endif()