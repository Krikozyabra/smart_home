find_package(ALSA REQUIRED)
find_package(Threads REQUIRED)

add_library(smart_home_audio STATIC
    src/AudioConfig.cpp
    src/AlsaSource.cpp
    src/AlsaAudioCapture.cpp)
add_library(SmartHome::Audio ALIAS smart_home_audio)
target_include_directories(smart_home_audio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(smart_home_audio PUBLIC cxx_std_17)
set_target_properties(smart_home_audio PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_compile_options(smart_home_audio PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(smart_home_audio PRIVATE ALSA::ALSA SmartHome::Logging Threads::Threads)

if(BUILD_TESTING)
    target_compile_definitions(smart_home_audio PRIVATE SMART_HOME_AUDIO_TESTING)
endif()
