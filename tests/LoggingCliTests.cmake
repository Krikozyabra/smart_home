execute_process(COMMAND "${SMART_HOME_EXE}" --log-level=info --log-file=
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics)
if(NOT status EQUAL 0 OR NOT output MATCHES "Temperature is 23" OR NOT output MATCHES "New brightness is 75")
    message(FATAL_ERROR "Measurements or exit status changed: ${output} ${diagnostics}")
endif()
if(output MATCHES "Application|\\[info\\]" OR NOT diagnostics MATCHES "Application started" OR NOT diagnostics MATCHES "Application finished")
    message(FATAL_ERROR "Diagnostics must appear on stderr only")
endif()
execute_process(COMMAND "${SMART_HOME_EXE}" --log-level=off --log-file=
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics)
if(NOT status EQUAL 0 OR NOT diagnostics STREQUAL "")
    message(FATAL_ERROR "OFF must suppress normal diagnostics")
endif()
execute_process(COMMAND "${SMART_HOME_EXE}" --log-level=invalid
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics)
if(NOT status EQUAL 2 OR NOT output STREQUAL "" OR NOT diagnostics MATCHES "Logging configuration error")
    message(FATAL_ERROR "Invalid CLI must fail before business work")
endif()
