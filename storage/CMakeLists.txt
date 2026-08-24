add_library(smart_home_storage STATIC
    src/InMemoryDeviceStorage.cpp
    src/SQLiteDeviceStorage.cpp
)

add_library(SmartHome::Storage ALIAS smart_home_storage)

target_include_directories(smart_home_storage PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(SQLite3 REQUIRED)

if(NOT TARGET SQLite3::SQLite3)
    add_library(SQLite3::SQLite3 ALIAS SQLite::SQLite3)
endif()

target_link_libraries(smart_home_storage PRIVATE
    SQLite3::SQLite3
)

target_compile_features(smart_home_storage PRIVATE
    cxx_std_17
)
