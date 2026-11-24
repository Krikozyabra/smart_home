option(SMART_HOME_BUILD_TTS "Build offline speech synthesis with ONNX Runtime" OFF)

set(SMART_HOME_TTS_MODEL_URL "" CACHE STRING "Optional URL of a compatible Silero ONNX export")
set(SMART_HOME_TTS_MODEL_SHA256 "" CACHE STRING "SHA-256 of SMART_HOME_TTS_MODEL_URL")
set(SMART_HOME_TTS_EXPORT_PYTHON "python3" CACHE FILEPATH "Python with PyTorch and ONNX for exporting Silero")
add_custom_target(tts_models
    COMMAND ${CMAKE_COMMAND} "-DTTS_MODELS_DIR=${CMAKE_CURRENT_BINARY_DIR}/models"
        "-DTTS_MODEL_URL=${SMART_HOME_TTS_MODEL_URL}"
        "-DTTS_MODEL_SHA256=${SMART_HOME_TTS_MODEL_SHA256}"
        "-DTTS_EXPORT_PYTHON=${SMART_HOME_TTS_EXPORT_PYTHON}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/DownloadModels.cmake
    COMMENT "Preparing the Silero Russian TTS ONNX model"
    VERBATIM
)

if(NOT SMART_HOME_BUILD_TTS)
    return()
endif()

add_library(smart_home_tts STATIC
    src/SileroTtsSynthesizer.cpp
    src/Tokenizer.cpp
    src/TtsConfig.cpp
)
add_library(SmartHome::Tts ALIAS smart_home_tts)
target_include_directories(smart_home_tts PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(smart_home_tts PUBLIC cxx_std_17)
set_target_properties(smart_home_tts PROPERTIES
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_compile_options(smart_home_tts PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(smart_home_tts PRIVATE SmartHome::Logging SmartHome::OnnxRuntime)
