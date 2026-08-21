add_library(smart_home_registry STATIC
    src/DeviceRegistry.cpp
)

add_library(SmartHome::Registry ALIAS smart_home_registry)

target_include_directories(smart_home_registry PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(smart_home_registry PUBLIC
    SmartHome::Devices
    SmartHome::Descriptors
    SmartHome::Drivers
)

target_compile_features(smart_home_registry PRIVATE
    cxx_std_17
)
