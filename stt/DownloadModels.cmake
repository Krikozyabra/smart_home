cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED STT_MODELS_DIR)
    set(STT_MODELS_DIR "${CMAKE_CURRENT_BINARY_DIR}/stt/models")
endif()
file(MAKE_DIRECTORY "${STT_MODELS_DIR}")
file(LOCK "${STT_MODELS_DIR}/.download.lock" GUARD PROCESS TIMEOUT 60)

# sherpa-onnx's GigaAM-v3 CTC export, pinned as a model/vocabulary pair.
set(model_url
    "https://huggingface.co/csukuangfj/sherpa-onnx-nemo-ctc-giga-am-v3-russian-2025-12-16/resolve/32a4c7cc81809bd132e2d935ab99e9e6ab47fbec")

function(download_model name remote_name sha256)
    set(destination "${STT_MODELS_DIR}/${name}")
    if(EXISTS "${destination}")
        file(SHA256 "${destination}" existing_hash)
        if(existing_hash STREQUAL sha256)
            message(STATUS "Using verified ${destination}")
            return()
        endif()
    endif()

    set(partial "${destination}.part")
    message(STATUS "Downloading ${name}")
    file(DOWNLOAD "${model_url}/${remote_name}" "${partial}"
        TLS_VERIFY ON TIMEOUT 900 INACTIVITY_TIMEOUT 60 STATUS status)
    list(GET status 0 code)
    if(NOT code EQUAL 0)
        file(REMOVE "${partial}")
        list(GET status 1 description)
        message(FATAL_ERROR "Download of ${name} failed: ${description}")
    endif()
    file(SHA256 "${partial}" downloaded_hash)
    if(NOT downloaded_hash STREQUAL sha256)
        file(REMOVE "${partial}")
        message(FATAL_ERROR "SHA256 mismatch for ${name}")
    endif()
    file(RENAME "${partial}" "${destination}")
endfunction()

download_model(gigaam_v3_ctc_int8.onnx model.int8.onnx
    f86ebfa0429ced91be6054fc344827e9c6c2572f3c318416cd974b06f66437ec)
download_model(tokens.txt tokens.txt
    17cc514451bcceac9c280068c71502f8448f99e9fb1456b8d0761651fd0392f2)
