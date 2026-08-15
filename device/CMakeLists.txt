add_library(smart_home_device STATIC
    src/*.cpp
)

add_library(SmartHome::Device ALIAS smart_home_device)

target_include_directories(smart_home_device PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(smart_home_device PRIVATE
    cxx_std_17
)
