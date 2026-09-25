add_library(smart_home_discovery STATIC
    src/SimulatedDiscoverySource.cpp
)

add_library(SmartHome::Discovery ALIAS smart_home_discovery)

target_include_directories(smart_home_discovery PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(smart_home_discovery PUBLIC
    SmartHome::DeviceManagement
)

target_compile_features(smart_home_devices PRIVATE
    cxx_std_17
)
