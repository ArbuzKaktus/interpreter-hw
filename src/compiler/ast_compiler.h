#ifndef HW1_SRC_COMPILER_AST_COMPILER_H_
#define HW1_SRC_COMPILER_AST_COMPILER_H_

#include <nlohmann/json.hpp>

#include "ir/ir.h"

ir::Program CompileAst(const nlohmann::json& ast);

#endif  // HW1_SRC_COMPILER_AST_COMPILER_H_
