cmake_minimum_required(VERSION 3.20)

project(smart_home
    VERSION 0.0.1
    LANGUAGES CXX C
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(device)
add_subdirectory(descriptors)
add_subdirectory(drivers)

add_subdirectory(src)
