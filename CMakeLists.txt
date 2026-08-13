cmake_minimum_required(VERSION 4.1)

project(smart_home
    VERSION 0.0.1
    LANGUAGES CXX C
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(src)
add_subdirectory(Device)
