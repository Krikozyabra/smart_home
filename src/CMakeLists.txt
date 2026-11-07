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
    SmartHome::Ontology
)

target_link_libraries(smart_home PRIVATE SmartHome::Logging)

option(SMART_HOME_BUILD_MQTT_DEMO "Build the standalone MQTT read/write demo" ON)
if(SMART_HOME_BUILD_MQTT_DEMO)
    add_executable(smart_home_mqtt_demo mqtt_demo.cpp)
    target_link_libraries(smart_home_mqtt_demo PRIVATE SmartHome::Mqtt SmartHome::Logging)
endif()

option(SMART_HOME_BUILD_AUDIO_OWW_DEMO "Build the audio/OpenWakeWord console demo" ON)
if(SMART_HOME_BUILD_AUDIO_OWW_DEMO)
    add_executable(smart_home_audio_oww_demo audio_oww_demo.cpp)
    target_link_libraries(smart_home_audio_oww_demo PRIVATE SmartHome::Audio SmartHome::Oww)
    set_target_properties(smart_home_audio_oww_demo PROPERTIES
        CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
    target_compile_options(smart_home_audio_oww_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()
