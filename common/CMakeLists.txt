add_library(smart_home_common INTERFACE)

add_library(SmartHome::Common ALIAS smart_home_common)

target_include_directories(smart_home_common INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(smart_home_common INTERFACE
    cxx_std_17
)
