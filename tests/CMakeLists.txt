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

add_executable(sqlite_dump_tests SQLiteDumpTests.cpp)
target_include_directories(sqlite_dump_tests PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(sqlite_dump_tests PRIVATE SmartHome::Storage SmartHome::Logging spdlog::spdlog)
set_target_properties(sqlite_dump_tests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
add_test(NAME sqlite_dump_tests COMMAND sqlite_dump_tests)
set_tests_properties(sqlite_dump_tests PROPERTIES TIMEOUT 30)

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

add_executable(oww_tests OwwTests.cpp)
target_link_libraries(oww_tests PRIVATE SmartHome::Oww)
set_target_properties(oww_tests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
add_test(NAME oww_tests COMMAND oww_tests ${CMAKE_CURRENT_SOURCE_DIR}/oww/models)
set_tests_properties(oww_tests PROPERTIES TIMEOUT 30)

add_executable(oww_model_tests OwwModelTests.cpp)
target_link_libraries(oww_model_tests PRIVATE SmartHome::Oww)
set(SMART_HOME_OWW_TEST_MODEL "" CACHE FILEPATH "Optional trained binary OpenWakeWord ONNX classifier for smoke tests")
set(SMART_HOME_OWW_TEST_MELSPEC "" CACHE FILEPATH "Optional melspectrogram.onnx path; empty uses classifier directory")
set(SMART_HOME_OWW_TEST_EMBEDDING "" CACHE FILEPATH "Optional embedding_model.onnx path; empty uses classifier directory")
if(SMART_HOME_OWW_TEST_MODEL)
    add_test(NAME oww_model_tests COMMAND oww_model_tests "${SMART_HOME_OWW_TEST_MODEL}"
        "${SMART_HOME_OWW_TEST_MELSPEC}" "${SMART_HOME_OWW_TEST_EMBEDDING}")
    set_tests_properties(oww_model_tests PROPERTIES TIMEOUT 180)
endif()

foreach(suite Config Capture Buffer Concurrency)
    string(TOLOWER "${suite}" suite_lower)
    set(target "audio_${suite_lower}_tests")
    add_executable(${target} Audio${suite}Tests.cpp)
    target_include_directories(${target} PRIVATE ${PROJECT_SOURCE_DIR}/audio/src)
    target_link_libraries(${target} PRIVATE SmartHome::Audio Threads::Threads)
    set_target_properties(${target} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES TIMEOUT 20)
endforeach()
target_link_libraries(audio_concurrency_tests PRIVATE SmartHome::Logging spdlog::spdlog)
# Header-only format compatibility check; Audio itself has no OWW dependency.
target_include_directories(audio_config_tests PRIVATE ${PROJECT_SOURCE_DIR}/oww/include)

set(SMART_HOME_AUDIO_TEST_DEVICE "" CACHE STRING "Explicit capture PCM for optional hardware tests")
if(SMART_HOME_AUDIO_TEST_DEVICE)
    add_executable(audio_alsa_tests AudioAlsaTests.cpp)
    target_link_libraries(audio_alsa_tests PRIVATE SmartHome::Audio Threads::Threads)
    add_test(NAME audio_alsa_tests COMMAND audio_alsa_tests "${SMART_HOME_AUDIO_TEST_DEVICE}")
    set_tests_properties(audio_alsa_tests PROPERTIES TIMEOUT 20 LABELS hardware)
endif()

if(TARGET smart_home_audio_oww_demo)
    add_test(NAME audio_oww_demo_help COMMAND smart_home_audio_oww_demo --help)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME audio_oww_demo_tests
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/AudioOwwDemoTests.py
                $<TARGET_FILE:smart_home_audio_oww_demo> ${CMAKE_CURRENT_SOURCE_DIR}/oww/models)
        set_tests_properties(audio_oww_demo_tests PROPERTIES TIMEOUT 60)
    endif()
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)
# Same composition as production; private fault hooks exist only in this executable.
add_executable(runtime_test_app ${PROJECT_SOURCE_DIR}/src/main.cpp)
get_target_property(runtime_libraries smart_home LINK_LIBRARIES)
target_link_libraries(runtime_test_app PRIVATE ${runtime_libraries})
target_compile_definitions(runtime_test_app PRIVATE SMART_HOME_RUNTIME_TESTING=1)
set_target_properties(runtime_test_app PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_compile_options(runtime_test_app PRIVATE -Wall -Wextra -Wpedantic)
foreach(suite console persistence events failures)
    add_test(NAME runtime_${suite}_tests
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/RuntimeTests.py
            $<TARGET_FILE:runtime_test_app> ${suite})
    set_tests_properties(runtime_${suite}_tests PROPERTIES TIMEOUT 120)
endforeach()
