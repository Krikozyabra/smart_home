option(SMART_HOME_FETCH_SPDLOG "Download pinned spdlog if no installed package is found" OFF)
find_package(spdlog CONFIG QUIET)
if(NOT TARGET spdlog::spdlog)
    if(NOT SMART_HOME_FETCH_SPDLOG)
        message(FATAL_ERROR "Install spdlog development files or set SMART_HOME_FETCH_SPDLOG=ON.")
    endif()
    include(FetchContent)
    set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.17.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(spdlog)
endif()

