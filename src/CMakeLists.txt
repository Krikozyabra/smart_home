add_executable(smart_home)

target_sources(smart_home PRIVATE main.cpp)

target_link_libraries(smart_home PRIVATE
    SmartHome::Devices
    SmartHome::Descriptors
    SmartHome::Drivers
    SmartHome::Registry
    SmartHome::Command
    SmartHome::Execution
    SmartHome::Storage
)

target_link_libraries(smart_home PRIVATE SmartHome::Logging)

option(SMART_HOME_BUILD_MQTT_DEMO "Build the standalone MQTT read/write demo" ON)
if(SMART_HOME_BUILD_MQTT_DEMO)
    add_executable(smart_home_mqtt_demo mqtt_demo.cpp)
    target_link_libraries(smart_home_mqtt_demo PRIVATE SmartHome::Mqtt SmartHome::Logging)
endif()
