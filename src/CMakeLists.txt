add_executable(smart_home)

target_sources(smart_home PRIVATE main.cpp)

target_compile_features(smart_home PUBLIC cxx_std_17)

target_link_libraries(smart_home PRIVATE SmartHome::device_lib)
