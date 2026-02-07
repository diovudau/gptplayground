cmake_minimum_required(VERSION 3.20)
project(ikigui_gain_clap LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(ikigui_gain_clap SHARED
    src/gain_plugin.cpp
)

target_include_directories(ikigui_gain_clap PRIVATE include)

target_compile_options(ikigui_gain_clap PRIVATE
    -Wall -Wextra -Wpedantic -Werror
    -fvisibility=hidden
)

target_link_options(ikigui_gain_clap PRIVATE
    -static-libstdc++
    -static-libgcc
)

set_target_properties(ikigui_gain_clap PROPERTIES
    PREFIX ""
    OUTPUT_NAME "ikigui_gain"
)
