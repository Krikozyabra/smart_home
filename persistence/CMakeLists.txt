find_package(SQLite3 REQUIRED)
if(NOT TARGET SQLite3::SQLite3)
    add_library(SQLite3::SQLite3 ALIAS SQLite::SQLite3)
endif()
find_package(OpenSSL REQUIRED COMPONENTS Crypto)
find_package(Threads REQUIRED)

option(SMART_HOME_RUNTIME_TESTING "Enable runtime fault injection in smart_home" OFF)

function(add_persistence_library target testing)
    add_library(${target} STATIC
        src/DeviceStateStore.cpp
        src/FileUtils.cpp
        src/GenerationManager.cpp
        src/PersistenceConfig.cpp
        src/PersistenceTypes.cpp
        src/Reservation.cpp
        src/SaveWorker.cpp
        src/StateLock.cpp
    )
    target_include_directories(${target} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(${target}
        PUBLIC SmartHome::Common SmartHome::Devices
        PRIVATE SQLite3::SQLite3 OpenSSL::Crypto Threads::Threads SmartHome::Logging
    )
    target_compile_features(${target} PUBLIC cxx_std_17)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    if(testing)
        target_sources(${target} PRIVATE src/Testing.cpp)
        target_compile_definitions(${target} PRIVATE SMART_HOME_RUNTIME_TESTING=1)
    endif()
endfunction()

add_persistence_library(smart_home_persistence ${SMART_HOME_RUNTIME_TESTING})
add_library(SmartHome::Persistence ALIAS smart_home_persistence)

if(BUILD_TESTING)
    if(SMART_HOME_RUNTIME_TESTING)
        add_library(SmartHome::PersistenceTesting ALIAS smart_home_persistence)
    else()
        add_persistence_library(smart_home_persistence_testing ON)
        add_library(SmartHome::PersistenceTesting ALIAS smart_home_persistence_testing)
    endif()
endif()
