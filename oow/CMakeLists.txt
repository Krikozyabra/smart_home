find_package(Threads REQUIRED)

add_library(smart_home_oow STATIC
    src/UserProfileCalibrator.cpp
    src/WakeWordService.cpp
)
add_library(SmartHome::OOW ALIAS smart_home_oow)

target_include_directories(smart_home_oow PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(smart_home_oow PUBLIC
    SmartHome::Common
    SmartHome::Logging
    Threads::Threads
)
