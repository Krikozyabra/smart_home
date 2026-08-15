add_library(smart_home_descriptors INTERFACE)

add_library(SmartHome::Descriptors ALIAS smart_home_descriptors)

target_include_directories(smart_home_descriptors INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(smart_home_descriptors INTERFACE
    cxx_std_17
)
