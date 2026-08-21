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
