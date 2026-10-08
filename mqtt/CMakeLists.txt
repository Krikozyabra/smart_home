find_package(PkgConfig REQUIRED)
pkg_check_modules(MOSQUITTOPP QUIET IMPORTED_TARGET libmosquittopp>=2.0)
pkg_check_modules(MOSQUITTO QUIET IMPORTED_TARGET libmosquitto>=2.0)
if(NOT MOSQUITTOPP_FOUND OR NOT MOSQUITTO_FOUND)
    message(FATAL_ERROR
        "MQTT requires libmosquittopp and libmosquitto development packages (>= 2.0). "
        "Install mosquitto on Arch/Manjaro or libmosquittopp-dev and libmosquitto-dev "
        "on Debian/Ubuntu, or set PKG_CONFIG_PATH for your installation.")
endif()
find_package(Threads REQUIRED)

add_library(smart_home_mqtt STATIC
    src/MqttConfig.cpp
    src/MosquittoGlobal.cpp
    src/MosquittoClient.cpp
)
add_library(SmartHome::Mqtt ALIAS smart_home_mqtt)
target_include_directories(smart_home_mqtt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(smart_home_mqtt PUBLIC cxx_std_17)
target_link_libraries(smart_home_mqtt PRIVATE
    PkgConfig::MOSQUITTOPP PkgConfig::MOSQUITTO Threads::Threads SmartHome::Logging)
