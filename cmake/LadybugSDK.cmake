if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "The RAM-first ontology currently requires Linux (flock, tmpfs, fsync).")
endif()

set(SMART_HOME_LADYBUG_SDK_DIR "$ENV{LADYBUG_SDK_DIR}" CACHE PATH "LadybugDB SDK root (include/lbug.hpp and lib/liblbug)")
if(NOT SMART_HOME_LADYBUG_SDK_DIR)
    message(FATAL_ERROR "Set SMART_HOME_LADYBUG_SDK_DIR or LADYBUG_SDK_DIR to a prebuilt LadybugDB SDK.")
endif()
find_path(SMART_HOME_LADYBUG_INCLUDE NAMES lbug.hpp
    PATHS "${SMART_HOME_LADYBUG_SDK_DIR}/include" "${SMART_HOME_LADYBUG_SDK_DIR}" NO_DEFAULT_PATH REQUIRED)
find_library(SMART_HOME_LADYBUG_LIBRARY NAMES lbug
    PATHS "${SMART_HOME_LADYBUG_SDK_DIR}/lib" "${SMART_HOME_LADYBUG_SDK_DIR}" NO_DEFAULT_PATH REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
add_library(Ladybug::SDK UNKNOWN IMPORTED)
set_target_properties(Ladybug::SDK PROPERTIES
    IMPORTED_LOCATION "${SMART_HOME_LADYBUG_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${SMART_HOME_LADYBUG_INCLUDE}"
    INTERFACE_LINK_LIBRARIES "OpenSSL::SSL;OpenSSL::Crypto;Threads::Threads;${CMAKE_DL_LIBS}")

# Compile, link and run against the chosen headers AND binary: detects ABI/link
# mismatches without imposing the SDK's language standard on public consumers.
if(CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "Ladybug SDK validation currently needs a native build.")
endif()
foreach(standard 17 20)
    try_run(ladybug_run ladybug_compile
        "${CMAKE_BINARY_DIR}/CMakeFiles/ladybug-probe-${standard}"
        "${CMAKE_CURRENT_LIST_DIR}/LadybugSdkProbe.cpp"
        CXX_STANDARD ${standard} CXX_STANDARD_REQUIRED ON
        LINK_LIBRARIES Ladybug::SDK
        COMPILE_OUTPUT_VARIABLE ladybug_compile_output
        RUN_OUTPUT_VARIABLE ladybug_version)
    if(ladybug_compile AND ladybug_run EQUAL 0)
        set(SMART_HOME_LADYBUG_CXX_STANDARD ${standard})
        string(STRIP "${ladybug_version}" ladybug_version)
        message(STATUS "Ladybug SDK ${ladybug_version}; private C++${standard} API validated")
        break()
    endif()
endforeach()
if(NOT SMART_HOME_LADYBUG_CXX_STANDARD)
    message(FATAL_ERROR "Ladybug SDK compile/link/runtime validation failed:\n${ladybug_compile_output}\n${ladybug_version}")
endif()
