file(MAKE_DIRECTORY "${TEST_ROOT}/config/FM" "${TEST_ROOT}/folder")
file(WRITE "${TEST_ROOT}/config/FM/FM.conf"
    "[workspace]\nleftViewMode=3\nrightViewMode=3\nleftPath=${TEST_ROOT}/folder\nrightPath=${TEST_ROOT}/folder\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "XDG_CONFIG_HOME=${TEST_ROOT}/config" "XDG_DATA_HOME=${TEST_ROOT}/data"
        "XDG_CACHE_HOME=${TEST_ROOT}/cache"
        QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software
        "${FM_EXECUTABLE}" --navigation-gui-benchmark --map-startup
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
    TIMEOUT 30)
if(NOT result EQUAL 0 OR NOT output MATCHES "\"startupViewModesPassed\":true")
    message(FATAL_ERROR "Restored Map startup failed: ${result}\n${output}\n${error}")
endif()
