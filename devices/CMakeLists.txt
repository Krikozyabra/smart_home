add_library(smart_home_devices STATIC
    src/Device.cpp
)

add_library(SmartHome::Devices ALIAS smart_home_devices)

target_include_directories(smart_home_devices PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(smart_home_devices PUBLIC
    SmartHome::Common
)

target_compile_features(smart_home_devices PUBLIC
    cxx_std_17
)
