#include "compiler/ast_compiler.h"

#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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
    } else if (expression.contains("array")) {
      RequireFields(expression, 1, "array");
      const auto& elements = expression.at("array");
      if (!elements.is_array())
        throw std::runtime_error("expected array elements");
      for (const auto& element : elements) CompileExpression(element);
      Emit(ir::Array{elements.size()});
    } else if (expression.contains("sexp")) {
      RequireFields(expression, 2, "sexp");
      const auto tag = RequireString(expression.at("sexp"), "sexp");
      if (tag.empty()) throw std::runtime_error("empty S-expression tag");
      const auto& arguments = expression.at("args");
      if (!arguments.is_array()) throw std::runtime_error("expected SEXP args");
      for (const auto& argument : arguments) CompileExpression(argument);
      Emit(ir::Sexp{tag, arguments.size()});
    } else if (expression.contains("string")) {
      RequireFields(expression, 1, "string");
      Emit(ir::String{RequireString(expression.at("string"), "string")});
    } else if (expression.contains("length")) {
      RequireFields(expression, 1, "length");
      CompileExpression(expression.at("length"));
      Emit(ir::Length{});
    } else if (expression.contains("elem")) {
      RequireFields(expression, 2, "elem");
      CompileExpression(expression.at("elem"));
      CompileExpression(expression.at("index"));
      Emit(ir::Elem{});
    } else if (expression.contains("binop")) {
      RequireFields(expression, 3, "binary expression");
      const auto symbol = RequireString(expression.at("binop"), "binop");
      if (symbol == ":") {
        CompileExpression(expression.at("left"));
        CompileExpression(expression.at("right"));
        Emit(ir::Sexp{"cons", 2});
        return;
      }
      const auto op = ParseOperator(symbol, false);
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
    } else if (tag == "set") {
      RequireFields(body, 3, "set");
      CompileExpression(body.at("dst"));
      CompileExpression(body.at("idx"));
      CompileExpression(body.at("value"));
      Emit(ir::SetElem{});
    } else if (tag == "if") {
      CompileIf(body);
    } else if (tag == "while" || tag == "do") {
      CompileLoop(body, tag == "do");
    } else if (tag == "switch") {
      if (target_ == AstTarget::kStackMachine) {
        throw std::runtime_error("SM output does not yet support patterns");
      }
      CompileSwitch(body);
    } else {
      throw std::runtime_error("unknown statement type");
    }
  }

  using Path = std::vector<std::size_t>;
  using Bindings = std::vector<std::pair<std::string, Path>>;

  void LoadPatternValue(const Path& path) {
    Emit(ir::Load{"_"});
    for (const auto index : path) {
      Emit(ir::Push{static_cast<std::int64_t>(index)});
      Emit(ir::Elem{});
    }
  }

  void CompilePattern(const json& pattern, Path& path,
                      std::vector<std::size_t>& failures, Bindings& bindings) {
    using Kind = ir::PatternKind;
    const auto test = [&](ir::PatternTest predicate) {
      LoadPatternValue(path);
      Emit(std::move(predicate));
      failures.push_back(Emit(ir::Branch{0, false}));
    };
    const auto equality = [&] {
      Emit(ir::Binary{ir::BinaryOp::kEq});
      failures.push_back(Emit(ir::Branch{0, false}));
    };

    if (pattern.is_string()) {
      const auto name = pattern.get<std::string>();
      if (name == "#int")
        test({Kind::kInt, {}});
      else if (name == "#string")
        test({Kind::kString, {}});
      else if (name == "#array")
        test({Kind::kArray, {}});
      else if (name == "#sexp")
        test({Kind::kSexp, {}});
      else if (name != "_") {
        LoadPatternValue(path);
        Emit(ir::String{name});
        equality();
      }
    } else if (pattern.is_number_integer()) {
      LoadPatternValue(path);
      CompileExpression(json{{"const", pattern}});
      equality();
    } else if (pattern.contains("wildcard")) {
      RequireFields(pattern, 1, "wildcard");
    } else if (pattern.contains("bind")) {
      RequireFields(pattern, 1, "bind pattern");
      const auto& bind = pattern.at("bind");
      RequireFields(bind, 2, "binding");
      CompilePattern(bind.at("pattern"), path, failures, bindings);
      const auto name = RequireString(bind.at("name"), "name");
      if (name.empty() || name == "_") {
        throw std::runtime_error("invalid pattern binding name");
      }
      bindings.emplace_back(name, path);
    } else if (pattern.contains("array") || pattern.contains("sexp")) {
      const bool sexp = pattern.contains("sexp");
      RequireFields(pattern, sexp ? 2 : 1, "composite pattern");
      const auto& children = pattern.at(sexp ? "args" : "array");
      if (!children.is_array())
        throw std::runtime_error("invalid pattern args");
      if (sexp) {
        test({Kind::kTag, RequireString(pattern.at("sexp"), "sexp"),
              children.size()});
      } else {
        test({Kind::kArray, {}});
        LoadPatternValue(path);
        Emit(ir::Length{});
        Emit(ir::Push{static_cast<std::int64_t>(children.size())});
        equality();
      }
      for (std::size_t i = 0; i < children.size(); ++i) {
        path.push_back(i);
        CompilePattern(children[i], path, failures, bindings);
        path.pop_back();
      }
    } else {
      throw std::runtime_error("unknown pattern type");
    }
  }

  void CompileSwitch(const json& body) {
    RequireFields(body, 2, "switch");
    const auto& branches = body.at("branches");
    if (!branches.is_array() || branches.empty()) {
      throw std::runtime_error("switch expects at least one branch");
    }
    CompileExpression(body.at("scrutinee"));
    Emit(ir::Scope{});
    Emit(ir::Store{"_"});
    Emit(ir::Enter{});
    std::vector<std::size_t> exits;
    for (const auto& branch : branches) {
      RequireFields(branch, 2, "pattern branch");
      std::vector<std::size_t> failures;
      Bindings bindings;
      Path path;
      CompilePattern(branch.at("pattern"), path, failures, bindings);
      Emit(ir::Scope{});
      for (const auto& [name, binding_path] : bindings) {
        LoadPatternValue(binding_path);
        Emit(ir::Store{name});
      }
      Emit(ir::Enter{});
      CompileStatement(branch.at("branch"));
      Emit(ir::Leave{});
      Emit(ir::Leave{});
      exits.push_back(Emit(ir::Jump{0}));
      for (const auto failure : failures) Patch<ir::Branch>(failure);
    }
    Emit(ir::Load{"_"});
    Emit(ir::MatchFailure{});
    for (const auto exit : exits) Patch<ir::Jump>(exit);
  }

  void CompileAssignment(const json& body) {
    const bool compound = body.contains("op");
    RequireFields(body, compound ? 3 : 2, "assn");
    const auto& dst = body.at("dst");
    if (dst.is_object()) RequireFields(dst, 1, "assignment destination");
    const auto destination =
        RequireString(dst.is_object() ? dst.at("var") : dst, "dst");
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
