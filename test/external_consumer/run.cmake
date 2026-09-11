execute_process(COMMAND "${CMAKE_COMMAND}"
    -S "${SOURCE_DIR}/test/external_consumer" -B "${BINARY_DIR}"
    "-DIMEDIAPLAYER_SOURCE_DIR=${SOURCE_DIR}" "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
    -DCMAKE_BUILD_TYPE=Release RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "External consumer configure failed: ${result}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${BINARY_DIR}"
    --target external_consumer --config Release --parallel 2 RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "External consumer build failed: ${result}")
endif()
if(EXISTS "${BINARY_DIR}/Release/external_consumer.exe")
    set(executable "${BINARY_DIR}/Release/external_consumer.exe")
elseif(EXISTS "${BINARY_DIR}/external_consumer.exe")
    set(executable "${BINARY_DIR}/external_consumer.exe")
else()
    set(executable "${BINARY_DIR}/external_consumer")
endif()
execute_process(COMMAND "${executable}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "External consumer run failed: ${result}")
endif()