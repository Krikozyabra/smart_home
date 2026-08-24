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


add_executable(registry_tests
    RegistryTests.cpp
)

target_link_libraries(registry_tests PRIVATE
    SmartHome::Registry
    SmartHome::Drivers
)

add_test(
    NAME registry_tests
    COMMAND registry_tests
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
