cmake_minimum_required(VERSION 3.20)

project(MusicPlayerTRM LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG v7.0.3
)

FetchContent_MakeAvailable(ftxui)

add_executable(MusicPlayerTRM
    src/main.cpp
    third_party/miniaudio.c
)

target_include_directories(MusicPlayerTRM PRIVATE
    ${ftxui_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_link_libraries(MusicPlayerTRM PRIVATE
    ftxui::ftxui
)

if(MINGW)
    target_link_options(MusicPlayerTRM PRIVATE
        -static
        -static-libgcc
        -static-libstdc++
    )
endif()