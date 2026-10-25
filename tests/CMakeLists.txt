add_executable(command_dispatcher_tests
CommandDispatcherTests.cpp
)

target_link_libraries(command_dispatcher_tests PRIVATE
SmartHome::Registry
SmartHome::Drivers
SmartHome::Execution
)

add_test(
NAME command_dispatcher_tests
COMMAND command_dispatcher_tests
)

add_executable(storage_tests
StorageTests.cpp
)

target_link_libraries(storage_tests
SmartHome::Storage
SmartHome::DeviceManagement
)

add_test(
NAME storage_tests
COMMAND storage_tests
)

add_executable(sql_storage_tests
SQLStorageTests.cpp
)

target_link_libraries(sql_storage_tests
SmartHome::Storage
SmartHome::DeviceManagement
)

add_test(
NAME sql_storage_tests
COMMAND sql_storage_tests
)

add_executable(factories_tests
FactoriesTests.cpp
)

target_link_libraries(factories_tests
SmartHome::Drivers
SmartHome::Storage
)

add_test(
NAME factories_tests
COMMAND factories_tests
)

add_executable(device_registry_tests
DeviceRegistryTests.cpp
)

target_link_libraries(device_registry_tests PRIVATE
SmartHome::Registry
SmartHome::Drivers
SmartHome::Devices
)

add_test(
NAME device_registry_tests
COMMAND device_registry_tests
)

add_executable(driver_factory_registry_tests
DriverFactoryRegistryTests.cpp
)

target_link_libraries(driver_factory_registry_tests PRIVATE
SmartHome::Registry
SmartHome::Drivers
)

add_test(
NAME driver_factory_registry_tests
COMMAND driver_factory_registry_tests
)

add_executable(device_manager_tests
DeviceManagerTests.cpp
)

target_link_libraries(device_manager_tests PRIVATE
SmartHome::Registry
SmartHome::Drivers
SmartHome::Storage
SmartHome::DeviceManagement
)

add_test(
NAME device_manager_tests
COMMAND device_manager_tests
)


add_executable(discovery_tests
    DiscoveryTests.cpp
)

target_link_libraries(discovery_tests PRIVATE
    SmartHome::Discovery
)

add_test(
    NAME discovery_tests
    COMMAND discovery_tests
)


add_executable(discovery_service_tests
    DiscoveryServiceTests.cpp
)

target_link_libraries(discovery_service_tests
    SmartHome::Discovery
    SmartHome::DeviceManagement
    SmartHome::Storage
    SmartHome::Drivers
    SmartHome::Registry
)

add_test(
    NAME discovery_service_tests
    COMMAND discovery_service_tests
)

add_executable(logging_tests LoggingTests.cpp)
target_include_directories(logging_tests PRIVATE ${PROJECT_SOURCE_DIR}/logging/src)
target_link_libraries(logging_tests PRIVATE SmartHome::Logging SmartHome::Discovery
    SmartHome::DeviceManagement SmartHome::Storage spdlog::spdlog)
add_test(NAME logging_tests COMMAND logging_tests)
add_test(NAME logging_cli_tests COMMAND ${CMAKE_COMMAND}
    -DSMART_HOME_EXE=$<TARGET_FILE:smart_home>
    -P ${CMAKE_CURRENT_SOURCE_DIR}/LoggingCliTests.cmake)

add_executable(mqtt_tests MqttTests.cpp)
target_include_directories(mqtt_tests PRIVATE ${PROJECT_SOURCE_DIR}/mqtt/src)
target_link_libraries(mqtt_tests PRIVATE SmartHome::Mqtt Threads::Threads)
add_test(NAME mqtt_tests COMMAND mqtt_tests)
set_tests_properties(mqtt_tests PROPERTIES TIMEOUT 15)

option(SMART_HOME_MQTT_INTEGRATION_TESTS "Test MQTT against a temporary loopback broker" ON)
if(SMART_HOME_MQTT_INTEGRATION_TESTS AND UNIX)
    find_program(SMART_HOME_MOSQUITTO_EXECUTABLE NAMES mosquitto)
    if(SMART_HOME_MOSQUITTO_EXECUTABLE)
        add_executable(mqtt_integration_tests MqttIntegrationTests.cpp)
        target_link_libraries(mqtt_integration_tests PRIVATE SmartHome::Mqtt Threads::Threads)
        set(MQTT_DEMO_TEST_ARGUMENT)
        if(TARGET smart_home_mqtt_demo)
            set(MQTT_DEMO_TEST_ARGUMENT $<TARGET_FILE:smart_home_mqtt_demo>)
        endif()
        add_test(NAME mqtt_integration_tests
            COMMAND mqtt_integration_tests ${SMART_HOME_MOSQUITTO_EXECUTABLE} ${MQTT_DEMO_TEST_ARGUMENT})
        set_tests_properties(mqtt_integration_tests PROPERTIES TIMEOUT 60)
    else()
        message(STATUS "Mosquitto broker executable not found; MQTT integration tests are disabled.")
    endif()
endif()

foreach(suite Config Storage Sync Concurrency)
    string(TOLOWER "${suite}" suite_lower)
    set(target "ontology_${suite_lower}_tests")
    add_executable(${target} Ontology${suite}Tests.cpp)
    target_include_directories(${target} PRIVATE ${PROJECT_SOURCE_DIR}/ontology/src)
    target_link_libraries(${target} PRIVATE SmartHome::Ontology Ladybug::SDK Threads::Threads)
    set_target_properties(${target} PROPERTIES CXX_STANDARD ${SMART_HOME_LADYBUG_CXX_STANDARD} CXX_STANDARD_REQUIRED ON)
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES TIMEOUT 60)
endforeach()

add_executable(oow_tests OOWTests.cpp)
target_include_directories(oow_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/logging/src
)
target_link_libraries(oow_tests PRIVATE SmartHome::OOW SmartHome::Logging spdlog::spdlog)
add_test(NAME oow_tests COMMAND oow_tests)
set_tests_properties(oow_tests PROPERTIES TIMEOUT 20)
