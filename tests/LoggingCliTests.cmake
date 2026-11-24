string(RANDOM LENGTH 12 ALPHABET abcdef0123456789 runtime_test_id)
set(runtime_test_dir "${CMAKE_CURRENT_BINARY_DIR}/runtime-cli-${runtime_test_id}")
set(runtime_ram_dir "/dev/shm/runtime-cli-${runtime_test_id}")
file(MAKE_DIRECTORY "${runtime_test_dir}" "${runtime_ram_dir}")
foreach(name SMART_HOME_ONTOLOGY_PATH SMART_HOME_ONTOLOGY_RAM_DIR SMART_HOME_ONTOLOGY_SYNC_SECONDS SMART_HOME_ONTOLOGY_ALLOW_DISK_FALLBACK)
    unset(ENV{${name}})
endforeach()
set(ENV{SMART_HOME_RUNTIME_STATE_DIR} "${runtime_test_dir}/state")
set(ENV{SMART_HOME_RUNTIME_RAM_DIR} "${runtime_ram_dir}")
file(WRITE "${runtime_test_dir}/input"
    "device 3 temperature.get\ndevice 1 brightness.set 75\ndevice 1 brightness.get\nexit\n")
execute_process(COMMAND "${SMART_HOME_EXE}" --log-level=info --log-file=
    INPUT_FILE "${runtime_test_dir}/input" TIMEOUT 30
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics)
if(NOT status EQUAL 0 OR NOT output MATCHES "temperature.get: 23" OR NOT output MATCHES "brightness.get: 75")
    message(FATAL_ERROR "Console or exit status changed: ${output} ${diagnostics}")
endif()
if(output MATCHES "Application|\\[info\\]" OR NOT diagnostics MATCHES "Application started" OR NOT diagnostics MATCHES "Application finished")
    message(FATAL_ERROR "Diagnostics must appear on stderr only")
endif()
execute_process(COMMAND "${SMART_HOME_EXE}" --log-level=off --log-file=
    INPUT_FILE "${runtime_test_dir}/input" TIMEOUT 30
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics)
if(NOT status EQUAL 0 OR NOT diagnostics STREQUAL "")
    message(FATAL_ERROR "OFF must suppress normal diagnostics: ${diagnostics}")
endif()
execute_process(COMMAND "${SMART_HOME_EXE}" --log-level=invalid
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics)
if(NOT status EQUAL 2 OR NOT output STREQUAL "" OR NOT diagnostics MATCHES "Logging configuration error")
    message(FATAL_ERROR "Invalid CLI must fail before business work")
endif()
execute_process(COMMAND "${SMART_HOME_EXE}" --log-level=off --log-file=
    "--runtime-state-dir=${runtime_test_dir}/override" INPUT_FILE "${runtime_test_dir}/input" TIMEOUT 30
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics)
if(NOT status EQUAL 0 OR NOT EXISTS "${runtime_test_dir}/override/CURRENT" OR NOT diagnostics STREQUAL "")
    message(FATAL_ERROR "Runtime CLI override failed: ${diagnostics}")
endif()
foreach(invalid --runtime-snapshot-capacity=-1 --runtime-snapshot-capacity=oops --runtime-snapshot-capacity=0 --runtime-state-dir= --runtime-unknown=1 --ontology-sync-seconds=0)
    execute_process(COMMAND "${SMART_HOME_EXE}" "${invalid}"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics)
    if(NOT status EQUAL 2 OR NOT output STREQUAL "" OR NOT diagnostics MATCHES "Runtime configuration error")
        message(FATAL_ERROR "Invalid runtime CLI was accepted: ${invalid}")
    endif()
endforeach()
file(REMOVE_RECURSE "${runtime_test_dir}" "${runtime_ram_dir}")
