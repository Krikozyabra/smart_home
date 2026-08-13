add_library(device_lib INTERFACE)

add_library(SmartHome::device_lib ALIAS device_lib)

target_include_directories(device_lib INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
