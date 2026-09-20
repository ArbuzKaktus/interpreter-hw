# Generic script for a successful integration test.
execute_process(
  COMMAND "${INTERPRETER}" "${AST}" --input "${INPUT}"
  RESULT_VARIABLE status
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "Interpreter failed: ${error}")
endif()
string(REPLACE "\n" "\\n" normalized_output "${output}")
if(NOT normalized_output STREQUAL "${EXPECTED}")
  message(FATAL_ERROR "Unexpected stdout. Expected: [${EXPECTED}], actual: [${output}]")
endif()
if(NOT error STREQUAL "")
  message(FATAL_ERROR "stderr must be empty on success: ${error}")
endif()
