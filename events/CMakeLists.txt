find_package(Threads REQUIRED)

add_library(smart_home_events STATIC
    src/EventBus.cpp
    src/Reactor.cpp
    src/Subscription.cpp
)
add_library(SmartHome::Events ALIAS smart_home_events)
target_include_directories(smart_home_events PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(smart_home_events PUBLIC cxx_std_17)
target_link_libraries(smart_home_events PRIVATE Threads::Threads)
set_target_properties(smart_home_events PROPERTIES
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_compile_options(smart_home_events PRIVATE -Wall -Wextra -Wpedantic)
