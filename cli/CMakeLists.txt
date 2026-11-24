add_library(smart_home_cli STATIC
    src/ActionCommand.cpp
    src/CliOutputSink.cpp
    src/CliProcessor.cpp
    src/DeviceCommands.cpp
    src/DeviceOutput.cpp
)
add_library(SmartHome::Cli ALIAS smart_home_cli)
target_include_directories(smart_home_cli PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(smart_home_cli PUBLIC cxx_std_17)
target_link_libraries(smart_home_cli
    PUBLIC SmartHome::Common SmartHome::Descriptors SmartHome::Devices
    PRIVATE SmartHome::Drivers
)
set_target_properties(smart_home_cli PROPERTIES
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_compile_options(smart_home_cli PRIVATE -Wall -Wextra -Wpedantic)
