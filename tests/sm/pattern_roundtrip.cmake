execute_process(
  COMMAND "${INTERPRETER}" --emit-sm "${AST}"
  RESULT_VARIABLE status OUTPUT_VARIABLE sm ERROR_VARIABLE error)
if(NOT status STREQUAL "0" OR NOT sm MATCHES "\"PATT\""
   OR NOT sm MATCHES "\"SCOPE\"" OR NOT sm MATCHES "\"MF\"")
  message(FATAL_ERROR "Pattern compilation failed: ${error} [${sm}]")
endif()
file(WRITE "${OUTPUT_FILE}" "${sm}")
execute_process(
  COMMAND "${INTERPRETER}" --sm "${OUTPUT_FILE}"
  RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT status STREQUAL "0" OR NOT output STREQUAL "5\n10\n7\n8\n")
  message(FATAL_ERROR "Compiled SM produced [${output}]: ${error}")
endif()
