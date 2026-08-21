cmake_minimum_required(VERSION 3.20)

project(smart_home
    VERSION 0.0.1
    LANGUAGES CXX C
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(CTest)

add_subdirectory(devices)
add_subdirectory(descriptors)
add_subdirectory(drivers)
add_subdirectory(registry)
add_subdirectory(command)
add_subdirectory(execution)

add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
