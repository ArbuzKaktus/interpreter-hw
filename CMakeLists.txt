cmake_minimum_required(VERSION 3.16)
project(ast_interpreter LANGUAGES CXX)

include(FetchContent)

# Show dependency download progress instead of silently waiting for GitHub.
set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    GIT_CONFIG http.lowSpeedLimit=1 http.lowSpeedTime=30
)
FetchContent_MakeAvailable(nlohmann_json)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(interpreter_ir src/ir/ir.cpp src/vm/vm.cpp)
target_include_directories(interpreter_ir PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(interpreter_ir PRIVATE -Wall -Wextra -Wpedantic)

add_executable(interpreter
    src/cli/main.cpp
    src/compiler/ast_compiler.cpp
    src/sm/sm_loader.cpp
    src/sm/sm_writer.cpp)
target_link_libraries(interpreter PRIVATE interpreter_ir)
target_link_libraries(interpreter PRIVATE nlohmann_json::nlohmann_json)
target_compile_options(interpreter PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()

add_test(
    NAME ast_to_sm_roundtrip
    COMMAND ${CMAKE_COMMAND}
        -DINTERPRETER=$<TARGET_FILE:interpreter>
        -DAST=${CMAKE_CURRENT_SOURCE_DIR}/tests/sample.json
        -DOUTPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/compiled_sample.json
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/sm/compiler_roundtrip.cmake)

function(add_interpreter_test name ast input expected)
    add_test(
        NAME ${name}
        COMMAND ${CMAKE_COMMAND}
            -DINTERPRETER=$<TARGET_FILE:interpreter>
            -DAST=${CMAKE_CURRENT_SOURCE_DIR}/tests/${ast}
            -DINPUT=${input}
            -DEXPECTED=${expected}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_case.cmake)
endfunction()

add_interpreter_test(sample_program sample.json 5 "15\\n")
add_interpreter_test(
    expressions
    expressions.json
    ""
    "10\\n4\\n21\\n2\\n1\\n1\\n0\\n1\\n1\\n1\\n0\\n1\\n1\\n"
)
add_interpreter_test(control_flow control_flow.json 3 "111\\n3\\n2\\n1\\n6\\n")
add_interpreter_test(compound_assignment compound.json "" "7\\n")
add_interpreter_test(
    sm_playground_export
    sm/control_flow.json
    7
    "6\\n4\\n2\\n1\\n1\\n"
)
add_interpreter_test(sm_reads_in_order sm/read_operands.json "7,2" "5\\n")
add_interpreter_test(
    sm_overflow_branch
    sm/overflow_branch.json
    ""
    "-2147483648\\n2147483647\\n-2147483648\\n"
)
add_interpreter_test(
    ast_integer_range
    ast_integer_range.json
    ""
    "2147483648\\n"
)
add_interpreter_test(
    sm_playground_operators_negative
    sm/playground_operators.json
    " -7 , 3 "
    "-4\\n-10\\n-21\\n-2\\n-1\\n0\\n1\\n1\\n1\\n0\\n0\\n1\\n1\\n"
)
add_interpreter_test(
    sm_playground_operators_equal
    sm/playground_operators.json
    "3,3"
    "6\\n0\\n9\\n1\\n0\\n1\\n0\\n0\\n1\\n0\\n1\\n1\\n1\\n"
)
add_interpreter_test(
    sm_playground_for_elif
    sm/playground_for_elif.json
    6
    "26\\n"
)
add_interpreter_test(
    sm_playground_for_zero_iterations
    sm/playground_for_elif.json
    0
    "0\\n"
)
add_interpreter_test(sm_playground_skip sm/empty.json "" "")
add_interpreter_test(
    sm_accepts_int32_min
    sm/read.json
    "-2147483648, 2147483647"
    "-2147483648\\n"
)

function(add_interpreter_failure_test name ast input expected_error)
    add_test(
        NAME ${name}
        COMMAND ${CMAKE_COMMAND}
            -DINTERPRETER=$<TARGET_FILE:interpreter>
            -DAST=${CMAKE_CURRENT_SOURCE_DIR}/tests/${ast}
            -DINPUT=${input}
            -DEXPECTED_ERROR=${expected_error}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/failure_case.cmake)
endfunction()

add_interpreter_failure_test(
    undefined_variable
    undefined_variable.json
    ""
    "uninitialized variable: x"
)
add_interpreter_failure_test(
    division_by_zero
    division_by_zero.json
    ""
    "division by zero"
)
add_interpreter_failure_test(
    missing_input
    missing_input.json
    ""
    "not enough input values"
)
add_interpreter_failure_test(
    malformed_ast
    malformed_ast.json
    ""
    "unknown statement type"
)
add_interpreter_failure_test(
    invalid_unreachable_ast
    invalid_unreachable.json
    ""
    "unknown statement type"
)
add_interpreter_failure_test(
    sm_unknown_label
    sm/unknown_label.json
    ""
    "unknown label: missing"
)
add_interpreter_failure_test(
    sm_invalid_constant
    sm/invalid_constant.json
    ""
    "CONST expects a signed 32-bit integer"
)
add_interpreter_failure_test(
    sm_stack_underflow
    sm/stack_underflow.json
    ""
    "stack underflow"
)
add_interpreter_failure_test(
    sm_missing_input
    sm/read_operands.json
    7
    "not enough input values for READ"
)
add_interpreter_failure_test(
    sm_discards_partial_output
    sm/partial_output.json
    ""
    "SM instruction 5: division by zero"
)
add_interpreter_failure_test(
    sm_duplicate_key
    sm/duplicate_key.json
    ""
    "duplicate JSON key: CONST"
)
add_interpreter_failure_test(
    sm_input_out_of_range
    sm/read.json
    2147483648
    "input value is outside the signed 32-bit range"
)
add_interpreter_failure_test(
    sm_unused_input_out_of_range
    sm/read.json
    "7,2147483648"
    "input value is outside the signed 32-bit range"
)
add_interpreter_failure_test(
    sm_trailing_input_comma
    sm/read.json
    "7,"
    "empty value in --input"
)
add_interpreter_failure_test(
    sm_input_leading_plus
    sm/read.json
    "+7"
    "invalid number in --input"
)
add_interpreter_failure_test(
    sm_eager_logical_error
    sm/eager_logical_error.json
    ""
    "SM instruction 3: division by zero"
)
add_interpreter_failure_test(
    sm_undefined_load
    sm/undefined_load.json
    ""
    "SM instruction 0: uninitialized variable: x"
)

add_test(
    NAME dump_ir
    COMMAND ${CMAKE_COMMAND}
        -DINTERPRETER=$<TARGET_FILE:interpreter>
        -DAST=${CMAKE_CURRENT_SOURCE_DIR}/tests/missing_input.json
        -DMODE=--dump-ir
        "-DEXPECTED=0: read \"x\"\\n1: end\\n"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_case.cmake)
