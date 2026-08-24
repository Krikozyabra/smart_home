add_library(smart_home_device_management STATIC
    src/DeviceManager.cpp
)

add_library(SmartHome::DeviceManagement ALIAS smart_home_device_management)

target_include_directories(smart_home_device_management PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(smart_home_device_management PUBLIC
    SmartHome::Storage
)

target_compile_features(smart_home_device_management PRIVATE
    cxx_std_17
)
