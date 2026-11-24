option(SMART_HOME_BUILD_STT "Build offline speech recognition with sherpa-onnx" OFF)

add_custom_target(stt_models
    COMMAND ${CMAKE_COMMAND} "-DSTT_MODELS_DIR=${CMAKE_CURRENT_BINARY_DIR}/models"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/DownloadModels.cmake
    COMMENT "Downloading the pinned GigaAM-v3 CTC model and vocabulary"
    VERBATIM
)

if(NOT SMART_HOME_BUILD_STT)
    return()
endif()

include(${PROJECT_SOURCE_DIR}/cmake/SherpaOnnx.cmake)

add_library(smart_home_stt STATIC
    src/ModelValidation.cpp
    src/SttConfig.cpp
    src/SherpaSttRecognizer.cpp
)
add_library(SmartHome::Stt ALIAS smart_home_stt)
target_include_directories(smart_home_stt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(smart_home_stt PUBLIC cxx_std_17)
set_target_properties(smart_home_stt PROPERTIES
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_compile_options(smart_home_stt PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(smart_home_stt
    PRIVATE SmartHome::Logging SmartHome::SherpaOnnx SmartHome::OnnxRuntime
)
