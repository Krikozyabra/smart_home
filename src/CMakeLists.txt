add_executable(smart_home)

target_sources(smart_home PRIVATE main.cpp)

target_link_libraries(smart_home PRIVATE
    SmartHome::Device
    SmartHome::Descriptors
    SmartHome::Drivers
)
