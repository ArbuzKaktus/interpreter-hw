execute_process(
  COMMAND "${INTERPRETER}" "${AST}" --emit-sm
  RESULT_VARIABLE status OUTPUT_VARIABLE sm ERROR_VARIABLE error)
if(NOT status STREQUAL "0" OR NOT error STREQUAL "")
  message(FATAL_ERROR "AST compilation failed: ${error}")
endif()
file(WRITE "${OUTPUT_FILE}" "${sm}")
execute_process(
  COMMAND "${INTERPRETER}" --sm "${OUTPUT_FILE}" --input 5
  RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT status STREQUAL "0" OR NOT output STREQUAL "15\n")
  message(FATAL_ERROR "Compiled SM produced [${output}]: ${error}")
endif()
if(NOT sm MATCHES "\"READ\""
   OR NOT sm MATCHES "\"ST\"[ \t]*:[ \t]*\"n\""
   OR NOT sm MATCHES "\"JNZ\"[ \t]*:[ \t]*\"L_[0-9]+\"")
  message(FATAL_ERROR "Unexpected SM layout: ${sm}")
endif()
