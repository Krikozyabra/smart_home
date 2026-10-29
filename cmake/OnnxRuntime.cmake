set(SMART_HOME_ONNXRUNTIME_DIR "$ENV{ONNXRUNTIME_ROOT}" CACHE PATH
    "ONNX Runtime SDK root (include/ and lib/); empty uses installed packages")
if(NOT SMART_HOME_ONNXRUNTIME_DIR)
    find_package(onnxruntime CONFIG QUIET)
endif()

if(NOT SMART_HOME_ONNXRUNTIME_DIR AND TARGET onnxruntime::onnxruntime)
    add_library(SmartHome::OnnxRuntime INTERFACE IMPORTED)
    set_target_properties(SmartHome::OnnxRuntime PROPERTIES
        INTERFACE_LINK_LIBRARIES onnxruntime::onnxruntime)
elseif(NOT SMART_HOME_ONNXRUNTIME_DIR AND TARGET onnxruntime)
    add_library(SmartHome::OnnxRuntime INTERFACE IMPORTED)
    set_target_properties(SmartHome::OnnxRuntime PROPERTIES INTERFACE_LINK_LIBRARIES onnxruntime)
else()
    # Official binary archives may contain headers/library without a CMake package.
    if(SMART_HOME_ONNXRUNTIME_DIR)
        # Do not silently combine an explicitly selected SDK with a system install.
        unset(SMART_HOME_ONNXRUNTIME_INCLUDE CACHE)
        unset(SMART_HOME_ONNXRUNTIME_LIBRARY CACHE)
        find_path(SMART_HOME_ONNXRUNTIME_INCLUDE NAMES onnxruntime_cxx_api.h
            PATHS "${SMART_HOME_ONNXRUNTIME_DIR}"
            PATH_SUFFIXES include include/onnxruntime include/onnxruntime/core/session NO_DEFAULT_PATH)
        find_library(SMART_HOME_ONNXRUNTIME_LIBRARY NAMES onnxruntime
            PATHS "${SMART_HOME_ONNXRUNTIME_DIR}" PATH_SUFFIXES lib lib64 NO_DEFAULT_PATH)
    else()
        find_path(SMART_HOME_ONNXRUNTIME_INCLUDE NAMES onnxruntime_cxx_api.h
            PATH_SUFFIXES onnxruntime onnxruntime/core/session)
        find_library(SMART_HOME_ONNXRUNTIME_LIBRARY NAMES onnxruntime)
    endif()
    if(NOT SMART_HOME_ONNXRUNTIME_INCLUDE OR NOT SMART_HOME_ONNXRUNTIME_LIBRARY)
        message(FATAL_ERROR
            "OWW requires the ONNX Runtime C++ SDK. Install its development package or set "
            "SMART_HOME_ONNXRUNTIME_DIR (or ONNXRUNTIME_ROOT) to an SDK with include/ and lib/.")
    endif()
    add_library(SmartHome::OnnxRuntime UNKNOWN IMPORTED)
    set_target_properties(SmartHome::OnnxRuntime PROPERTIES
        IMPORTED_LOCATION "${SMART_HOME_ONNXRUNTIME_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SMART_HOME_ONNXRUNTIME_INCLUDE}")
endif()
