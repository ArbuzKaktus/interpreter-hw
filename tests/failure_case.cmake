# Generic script for a failing integration test.
execute_process(
  COMMAND "${INTERPRETER}" "${AST}" --input "${INPUT}"
  RESULT_VARIABLE status
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error)
if(NOT status STREQUAL "1" OR NOT output STREQUAL "")
  message(
      FATAL_ERROR
      "Expected exit code 1 and empty stdout, got ${status}: [${output}]"
  )
endif()
string(FIND "${error}" "${EXPECTED_ERROR}" position)
if(position EQUAL -1)
  message(FATAL_ERROR "Expected error [${EXPECTED_ERROR}], actual: [${error}]")
endif()
