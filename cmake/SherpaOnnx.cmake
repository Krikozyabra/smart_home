option(SMART_HOME_FETCH_SHERPA_ONNX "Build the pinned sherpa-onnx SDK when it is not installed" OFF)
set(SMART_HOME_SHERPA_ONNX_DIR "" CACHE PATH
    "Root of a sherpa-onnx 2.0.1 shared C API SDK with include/ and lib/")

function(smart_home_find_sherpa_onnx)
    # The shared C API contains its transitive sherpa dependencies.
    set(CMAKE_FIND_LIBRARY_SUFFIXES "${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(search_paths)
    if(SMART_HOME_SHERPA_ONNX_DIR)
        set(search_paths PATHS "${SMART_HOME_SHERPA_ONNX_DIR}" NO_DEFAULT_PATH)
    endif()
    unset(SMART_HOME_SHERPA_ONNX_INCLUDE CACHE)
    unset(SMART_HOME_SHERPA_ONNX_LIBRARY CACHE)
    find_path(SMART_HOME_SHERPA_ONNX_INCLUDE NAMES sherpa-onnx/c-api/c-api.h
        PATH_SUFFIXES include ${search_paths})
    find_library(SMART_HOME_SHERPA_ONNX_LIBRARY NAMES sherpa-onnx-c-api
        PATH_SUFFIXES lib lib64 ${search_paths})
    if(SMART_HOME_SHERPA_ONNX_INCLUDE AND SMART_HOME_SHERPA_ONNX_LIBRARY)
        add_library(SmartHome::SherpaOnnx SHARED IMPORTED GLOBAL)
        set_target_properties(SmartHome::SherpaOnnx PROPERTIES
            IMPORTED_LOCATION "${SMART_HOME_SHERPA_ONNX_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${SMART_HOME_SHERPA_ONNX_INCLUDE}")
    endif()
endfunction()

function(smart_home_fetch_sherpa_onnx)
    include(FetchContent)
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    # Scope upstream options and the ONNX Runtime override to this subproject.
    list(PREPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/sherpa")
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(BUILD_SHARED_LIBS ON)
    set(project_build_testing "${BUILD_TESTING}")
    set(BUILD_TESTING OFF)
    set(SHERPA_ONNX_ENABLE_C_API ON)
    set(SHERPA_ONNX_ENABLE_BINARY OFF)
    set(SHERPA_ONNX_BUILD_C_API_EXAMPLES OFF)
    set(SHERPA_ONNX_ENABLE_PORTAUDIO OFF)
    set(SHERPA_ONNX_ENABLE_WEBSOCKET OFF)
    set(SHERPA_ONNX_ENABLE_TTS OFF)
    set(SHERPA_ONNX_ENABLE_SPEAKER_DIARIZATION OFF)
    set(SHERPA_ONNX_ENABLE_TESTS OFF)
    set(SHERPA_ONNX_ENABLE_GPU OFF)
    FetchContent_Declare(sherpa_onnx
        URL https://codeload.github.com/k2-fsa/sherpa-onnx/tar.gz/refs/tags/v2.0.1
        URL_HASH SHA256=24ecc0fccf6344c2556e7c229c849e9f61bf9456b915615197d85f35e3f177e7
    )
    FetchContent_MakeAvailable(sherpa_onnx)
    # Eigen declares a cache entry for BUILD_TESTING even in a subproject.
    set(BUILD_TESTING "${project_build_testing}" CACHE BOOL "Build the testing tree." FORCE)
    add_library(SmartHome::SherpaOnnx ALIAS sherpa-onnx-c-api)
endfunction()

smart_home_find_sherpa_onnx()
if(NOT TARGET SmartHome::SherpaOnnx)
    if(SMART_HOME_SHERPA_ONNX_DIR OR NOT SMART_HOME_FETCH_SHERPA_ONNX)
        message(FATAL_ERROR
            "STT requires the sherpa-onnx 2.0.1 shared C API and headers. "
            "Set SMART_HOME_SHERPA_ONNX_DIR to a complete SDK or enable SMART_HOME_FETCH_SHERPA_ONNX.")
    endif()
    smart_home_fetch_sherpa_onnx()
endif()
