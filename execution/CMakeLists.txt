add_library(smart_home_execution STATIC
    src/CommandDispatcher.cpp
    src/CommandValidation.cpp
    src/handlers/ColorHandlers.cpp
    src/handlers/OnOffHandlers.cpp
    src/handlers/BrightnessHandlers.cpp
    src/handlers/TemperatureHandlers.cpp
)

add_library(SmartHome::Execution ALIAS smart_home_execution)

target_include_directories(smart_home_execution PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(smart_home_execution PUBLIC
    SmartHome::Descriptors
    SmartHome::Devices
    SmartHome::Drivers
    SmartHome::Command
    SmartHome::Registry
)

target_compile_features(smart_home_execution PRIVATE
    cxx_std_17
)
