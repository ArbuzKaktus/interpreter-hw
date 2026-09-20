cmake_minimum_required(VERSION 3.16)
project(ast_interpreter LANGUAGES CXX)

include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(interpreter main.cpp)
target_link_libraries(interpreter PRIVATE nlohmann_json::nlohmann_json)
target_compile_options(interpreter PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()

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
add_interpreter_test(expressions expressions.json "" "10\\n4\\n21\\n2\\n1\\n1\\n0\\n1\\n1\\n1\\n0\\n1\\n1\\n")
add_interpreter_test(control_flow control_flow.json 3 "111\\n3\\n2\\n1\\n6\\n")
add_interpreter_test(compound_assignment compound.json "" "7\\n")

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

add_interpreter_failure_test(undefined_variable undefined_variable.json "" "uninitialized variable: x")
add_interpreter_failure_test(division_by_zero division_by_zero.json "" "division by zero")
add_interpreter_failure_test(missing_input missing_input.json "" "not enough input values")
add_interpreter_failure_test(malformed_ast malformed_ast.json "" "unknown statement type")
