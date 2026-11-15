#include "compiler/ast_compiler.h"

#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using nlohmann::json;

void RequireFields(const json& node, std::size_t count, const char* context) {
  if (!node.is_object() || node.size() != count) {
    throw std::runtime_error(std::string("invalid fields in ") + context);
  }
}

std::string RequireString(const json& value, const char* field) {
  if (!value.is_string()) {
    throw std::runtime_error(std::string(field) + " must be a string");
  }
  return value.get<std::string>();
}

ir::BinaryOp ParseOperator(const std::string& symbol, bool compound) {
  const auto op = ir::FindBinaryOp(symbol);
  if (op && (!compound || ir::IsArithmetic(*op))) return *op;
  throw std::runtime_error((compound ? "invalid compound assignment operator: "
                                     : "unknown binary operator: ") +
                           symbol);
}

class Compiler {
 public:
  explicit Compiler(AstTarget target) : target_(target) {}

  ir::Program Compile(const json& ast) {
    CompileStatement(ast);
    return std::move(program_);
  }

 private:
  std::size_t Emit(ir::Instruction instruction) {
    program_.push_back(std::move(instruction));
    return program_.size() - 1;
  }

  template <typename JumpType>
  void Patch(std::size_t index) {
    std::get<JumpType>(program_[index]).target = program_.size();
  }

  void CompileExpression(const json& expression) {
    if (expression.contains("const")) {
      RequireFields(expression, 1, "constant");
      const auto& value = expression.at("const");
      if (!value.is_number_integer() ||
          (value.is_number_unsigned() &&
           value.get<std::uint64_t>() >
               std::numeric_limits<std::int64_t>::max())) {
        throw std::runtime_error("const must be a signed 64-bit integer");
      }
      Emit(ir::Push{value.get<std::int64_t>()});
    } else if (expression.contains("var")) {
      RequireFields(expression, 1, "variable");
      Emit(ir::Load{RequireString(expression.at("var"), "var")});
    } else if (expression.contains("binop")) {
      RequireFields(expression, 3, "binary expression");
      const auto op =
          ParseOperator(RequireString(expression.at("binop"), "binop"), false);
      CompileExpression(expression.at("left"));
      if (target_ == AstTarget::kInterpreter &&
          (op == ir::BinaryOp::kAnd || op == ir::BinaryOp::kOr)) {
        const bool short_circuit_value = op == ir::BinaryOp::kOr;
        const auto branch = Emit(ir::Branch{0, short_circuit_value});
        CompileExpression(expression.at("right"));
        Emit(ir::ToBool{});
        const auto skip_constant = Emit(ir::Jump{0});
        Patch<ir::Branch>(branch);
        Emit(ir::Push{short_circuit_value});
        Patch<ir::Jump>(skip_constant);
      } else {
        CompileExpression(expression.at("right"));
        Emit(ir::Binary{op});
      }
    } else {
      throw std::runtime_error("unknown expression type");
    }
  }

  void CompileStatement(const json& statement) {
    if (statement.is_string() && statement == "skip") return;
    RequireFields(statement, 1, "statement");
    const auto node = statement.begin();
    const auto& tag = node.key();
    const auto& body = node.value();
    if (tag == "seq") {
      RequireFields(body, 2, "seq");
      CompileStatement(body.at("left"));
      CompileStatement(body.at("right"));
    } else if (tag == "read") {
      Emit(ir::Read{RequireString(body, "read")});
    } else if (tag == "write") {
      CompileExpression(body);
      Emit(ir::Write{});
    } else if (tag == "assn") {
      CompileAssignment(body);
    } else if (tag == "if") {
      CompileIf(body);
    } else if (tag == "while" || tag == "do") {
      CompileLoop(body, tag == "do");
    } else {
      throw std::runtime_error("unknown statement type");
    }
  }

  void CompileAssignment(const json& body) {
    const bool compound = body.contains("op");
    RequireFields(body, compound ? 3 : 2, "assn");
    const auto destination = RequireString(body.at("dst"), "dst");
    if (compound)
      Emit(ir::Load{destination, target_ == AstTarget::kInterpreter});
    CompileExpression(body.at("src"));
    if (compound) {
      Emit(ir::Binary{ParseOperator(RequireString(body.at("op"), "op"),
                                    target_ == AstTarget::kInterpreter)});
    }
    Emit(ir::Store{destination});
  }

  void CompileIf(const json& body) {
    const bool has_else = body.contains("else");
    RequireFields(body, has_else ? 3 : 2, "if");
    CompileExpression(body.at("cond"));
    const auto branch = Emit(ir::Branch{0, false});
    CompileStatement(body.at("then"));
    if (has_else) {
      const auto skip_else = Emit(ir::Jump{0});
      Patch<ir::Branch>(branch);
      CompileStatement(body.at("else"));
      Patch<ir::Jump>(skip_else);
    } else {
      Patch<ir::Branch>(branch);
    }
  }

  void CompileLoop(const json& body, bool post_test) {
    RequireFields(body, 2, post_test ? "do" : "while");
    const auto start = program_.size();
    if (post_test) {
      CompileStatement(body.at("body"));
      CompileExpression(body.at("cond"));
      Emit(ir::Branch{start, true});
    } else if (target_ == AstTarget::kStackMachine) {
      const auto condition = Emit(ir::Jump{0});
      const auto loop_body = program_.size();
      CompileStatement(body.at("body"));
      Patch<ir::Jump>(condition);
      CompileExpression(body.at("cond"));
      Emit(ir::Branch{loop_body, true});
    } else {
      CompileExpression(body.at("cond"));
      const auto exit = Emit(ir::Branch{0, false});
      CompileStatement(body.at("body"));
      Emit(ir::Jump{start});
      Patch<ir::Branch>(exit);
    }
  }

  AstTarget target_;
  ir::Program program_;
};

}  // namespace

ir::Program CompileAst(const nlohmann::json& ast, AstTarget target) {
  return Compiler(target).Compile(ast);
}
