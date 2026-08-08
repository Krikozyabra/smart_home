add_executable(smart_home)

target_sources(smart_home PRIVATE main.cpp)

target_compile_features(smart_home PUBLIC cxx_std_17)
