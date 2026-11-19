find_package(PkgConfig REQUIRED)
pkg_check_modules(JSON_C REQUIRED IMPORTED_TARGET json-c>=0.15)

add_library(smart_home_drivers STATIC
    src/CapabilityCatalog.cpp
    src/UniversalDeviceDriver.cpp
    src/Zigbee2Mqtt.cpp
)

add_library(SmartHome::Drivers ALIAS smart_home_drivers)

target_include_directories(smart_home_drivers PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(smart_home_drivers PUBLIC
    SmartHome::Devices
    SmartHome::Descriptors
    SmartHome::Storage
    SmartHome::Mqtt
)
target_link_libraries(smart_home_drivers PRIVATE PkgConfig::JSON_C)

target_compile_features(smart_home_drivers PUBLIC
    cxx_std_17
)
