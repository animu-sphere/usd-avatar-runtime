set(prefix "${ROOT_BINARY}/installed-vrm-usd/prefix")
set(consumer_binary "${ROOT_BINARY}/installed-vrm-usd/consumer")
if(NOT CONFIG)
    set(CONFIG Release)
endif()
function(run)
    execute_process(COMMAND ${ARGV} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Command failed (${result}): ${ARGV}\n${output}\n${error}")
    endif()
endfunction()
run("${CMAKE_COMMAND}" --install "${ROOT_BINARY}" --prefix "${prefix}" --config "${CONFIG}")
set(generator_args -G "${GENERATOR}")
if(PLATFORM)
    list(APPEND generator_args -A "${PLATFORM}")
endif()
run("${CMAKE_COMMAND}" -S "${CONSUMER_SOURCE}" -B "${consumer_binary}" ${generator_args}
    "-DCMAKE_PREFIX_PATH=${prefix}" "-DCMAKE_BUILD_TYPE=${CONFIG}"
    "-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}" "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
    "-DCMAKE_RC_COMPILER=${RC_COMPILER}" "-DCMAKE_MT=${MT}"
    "-DmotionSampling_DIR=${SAMPLING_DIR}" "-DmotionRetarget_DIR=${RETARGET_DIR}"
    "-DmotionCore_DIR=${MOTION_DIR}" "-Dpxr_DIR=${USD_DIR}"
    "-DvrmSchema_DIR=${SCHEMA_DIR}" "-DWITH_MOTION=${WITH_MOTION}")
run("${CMAKE_COMMAND}" --build "${consumer_binary}" --config "${CONFIG}")
run("${CMAKE_CTEST_COMMAND}" --test-dir "${consumer_binary}" -C "${CONFIG}" --output-on-failure)
