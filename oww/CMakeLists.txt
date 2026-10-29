add_library(smart_home_oww STATIC
    src/OwwConfig.cpp
    src/OpenWakeWordDetector.cpp)
add_library(SmartHome::Oww ALIAS smart_home_oww)
target_include_directories(smart_home_oww PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(smart_home_oww PUBLIC cxx_std_17)
set_target_properties(smart_home_oww PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_compile_options(smart_home_oww PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(smart_home_oww PUBLIC SmartHome::Common
    PRIVATE SmartHome::Logging SmartHome::OnnxRuntime)

# Deliberately opt-in: configuring/building the module never downloads models.
add_custom_target(oww_models
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/DownloadModels.cmake
    COMMENT "Downloading Nafanya and the two OpenWakeWord feature models"
    VERBATIM)
