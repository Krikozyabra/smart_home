add_library(smart_home_command STATIC
    src/Command.cpp
)

add_library(SmartHome::Command ALIAS smart_home_command)

target_include_directories(smart_home_command PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(smart_home_command PRIVATE
    SmartHome::Devices
)

target_compile_features(smart_home_command PRIVATE
    cxx_std_17
)
