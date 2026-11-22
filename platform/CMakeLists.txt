add_library(smart_home_platform STATIC
    src/DescriptorFlags.cpp
    src/DescriptorIO.cpp
    src/EventPipe.cpp
    src/FileDescriptor.cpp
    src/Poller.cpp
    src/SignalHandler.cpp
    src/SystemError.cpp
    src/TemporaryDirectory.cpp
)
add_library(SmartHome::Platform ALIAS smart_home_platform)
target_include_directories(smart_home_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(smart_home_platform PUBLIC cxx_std_17)
set_target_properties(smart_home_platform PROPERTIES
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_compile_options(smart_home_platform PRIVATE -Wall -Wextra -Wpedantic)
