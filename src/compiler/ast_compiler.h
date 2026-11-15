#ifndef HW1_SRC_COMPILER_AST_COMPILER_H_
#define HW1_SRC_COMPILER_AST_COMPILER_H_

#include <nlohmann/json.hpp>

#include "ir/ir.h"

enum class AstTarget { kInterpreter, kStackMachine };
ir::Program CompileAst(
    const nlohmann::json& ast,
    AstTarget target = AstTarget::kInterpreter);

#endif  // HW1_SRC_COMPILER_AST_COMPILER_H_
