# sherpa-onnx's include(onnxruntime) resolves here during FetchContent.
# Share the runtime selected by the project, including with OpenWakeWord.
if(NOT TARGET onnxruntime)
    add_library(onnxruntime INTERFACE IMPORTED)
    target_link_libraries(onnxruntime INTERFACE SmartHome::OnnxRuntime)
endif()
