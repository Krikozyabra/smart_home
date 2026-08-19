add_library(smart_home_drivers STATIC
    src/SimulatedLightDriver.cpp
    src/SimulatedTemperatureSensorDriver.cpp
)

add_library(SmartHome::Drivers ALIAS smart_home_drivers)

target_include_directories(smart_home_drivers PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(smart_home_drivers PRIVATE
    SmartHome::Devices
    SmartHome::Descriptors
)

target_compile_features(smart_home_drivers PRIVATE
    cxx_std_17
)
