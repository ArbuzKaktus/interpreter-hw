// Interpreter for the educational imperative language.

#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <unoredered_map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

using Json = nlohmann::json;

class Interpreter {
 public:
  explicit Interpreter(std::vector<std::int64_t> input)
      : input_(std::move(input)) {}

  void Run(const Json& program) { ExecuteStatement(program); }

 private:
  std::map<std::string, std::int64_t> variables_;
  std::vector<std::int64_t> input_;
  std::size_t input_position_ = 0;

  static void RequireObject(const Json& value, const std::string& name) {
    if (!value.is_object()) {
      throw std::runtime_error(name + " must be a JSON object");
    }
  }

  static const Json& Payload(const Json& node, const std::string& tag) {
    return node.at(tag);
  }

  static std::string RequireString(const Json& value,
                                   const std::string& field_name) {
    if (!value.is_string()) {
      throw std::runtime_error(field_name + " must be a string");
    }
    return value.get<std::string>();
  }

  std::int64_t EvaluateExpression(const Json& expression) {
    RequireObject(expression, "expression");
    if (expression.contains("const")) {
      return Payload(expression, "const").get<std::int64_t>();
    }
    if (expression.contains("var")) {
      const std::string name =
          RequireString(Payload(expression, "var"), "variable name");
      const auto iterator = variables_.find(name);
      if (iterator == variables_.end()) {
        throw std::runtime_error("uninitialized variable: " + name);
      }
      return iterator->second;
    }
    if (!expression.contains("binop")) {
      throw std::runtime_error("unknown expression type");
    }

    const std::string operation =
        RequireString(Payload(expression, "binop"), "binop");
    const std::int64_t left = EvaluateExpression(expression.at("left"));

    // Logical operations use short-circuit evaluation.
    if (operation == "&&") {
      return left != 0 && EvaluateExpression(expression.at("right")) != 0;
    }
    if (operation == "!!") {
      return left != 0 || EvaluateExpression(expression.at("right")) != 0;
    }

    const std::int64_t right = EvaluateExpression(expression.at("right"));
    if (operation == "+") {
      return left + right;
    }
    if (operation == "-") {
      return left - right;
    }
    if (operation == "*") {
      return left * right;
    }
    if (operation == "/") {
      if (right == 0) {
        throw std::runtime_error("division by zero");
      }
      return left / right;
    }
    if (operation == "%") {
      if (right == 0) {
        throw std::runtime_error("remainder by zero");
      }
      return left % right;
    }
    if (operation == "==") {
      return left == right;
    }
    if (operation == "!=") {
      return left != right;
    }
    if (operation == "<") {
      return left < right;
    }
    if (operation == "<=") {
      return left <= right;
    }
    if (operation == ">") {
      return left > right;
    }
    if (operation == ">=") {
      return left >= right;
    }
    throw std::runtime_error("unknown binary operator: " + operation);
  }

  void ExecuteAssignment(const Json& assignment) {
    RequireObject(assignment, "assignment");
    const std::string destination = RequireString(assignment.at("dst"), "dst");
    const std::int64_t source = EvaluateExpression(assignment.at("src"));
    if (!assignment.contains("op")) {
      variables_[destination] = source;
      return;
    }

    // The optional op field represents x += expression and similar forms.
    const std::int64_t old_value =
        variables_.count(destination) == 0 ? 0 : variables_[destination];
    const std::string operation = RequireString(assignment.at("op"), "op");
    if (operation == "+") {
      variables_[destination] = old_value + source;
    } else if (operation == "-") {
      variables_[destination] = old_value - source;
    } else if (operation == "*") {
      variables_[destination] = old_value * source;
    } else if (operation == "/") {
      if (source == 0) {
        throw std::runtime_error("division by zero");
      }
      variables_[destination] = old_value / source;
    } else if (operation == "%") {
      if (source == 0) {
        throw std::runtime_error("remainder by zero");
      }
      variables_[destination] = old_value % source;
    } else {
      throw std::runtime_error("invalid compound assignment operator: " +
                               operation);
    }
  }

  void ExecuteStatement(const Json& statement) {
    if (statement.is_string() && statement.get<std::string>() == "skip") {
      return;
    }
    RequireObject(statement, "statement");
    if (statement.contains("seq")) {
      const Json& sequence = Payload(statement, "seq");
      RequireObject(sequence, "seq");
      ExecuteStatement(sequence.at("left"));
      ExecuteStatement(sequence.at("right"));
      return;
    }
    if (statement.contains("read")) {
      const std::string name = RequireString(Payload(statement, "read"), "read");
      if (input_position_ == input_.size()) {
        throw std::runtime_error("not enough input values for read(" + name +
                                 ")");
      }
      variables_[name] = input_[input_position_++];
      return;
    }
    if (statement.contains("write")) {
      std::cout << EvaluateExpression(Payload(statement, "write")) << '\n';
      return;
    }
    if (statement.contains("assn")) {
      ExecuteAssignment(Payload(statement, "assn"));
      return;
    }
    if (statement.contains("if")) {
      const Json& conditional = Payload(statement, "if");
      RequireObject(conditional, "if");
      if (EvaluateExpression(conditional.at("cond")) != 0) {
        ExecuteStatement(conditional.at("then"));
      } else if (conditional.contains("else")) {
        ExecuteStatement(conditional.at("else"));
      }
      return;
    }
    if (statement.contains("while")) {
      const Json& loop = Payload(statement, "while");
      RequireObject(loop, "while");
      while (EvaluateExpression(loop.at("cond")) != 0) {
        ExecuteStatement(loop.at("body"));
      }
      return;
    }
    if (statement.contains("do")) {
      const Json& loop = Payload(statement, "do");
      RequireObject(loop, "do");
      do {
        ExecuteStatement(loop.at("body"));
      } while (EvaluateExpression(loop.at("cond")) != 0);
      return;
    }
    throw std::runtime_error("unknown statement type");
  }
};

std::vector<std::int64_t> ParseInput(const std::string& text) {
  std::vector<std::int64_t> values;
  std::stringstream stream(text);
  std::string part;
  while (std::getline(stream, part, ',')) {
    if (part.empty()) {
      throw std::runtime_error("empty value in --input");
    }
    std::size_t parsed_characters = 0;
    const std::int64_t value = std::stoll(part, &parsed_characters);
    if (parsed_characters != part.size()) {
      throw std::runtime_error("invalid number in --input: " + part);
    }
    values.push_back(value);
  }
  return values;
}

enum ExitCodes { SUCCESS, FAIL };

int main(int argc, char* argv[]) {
  try {
    std::string ast_path;
    std::vector<std::int64_t> input;
    for (int index = 1; index < argc; ++index) {
      const std::string argument = argv[index];
      if (argument == "--help") {
        std::cout << "Usage: interpreter [ast.json] [--input 1,2,3]\n";
        return ExitCodes::SUCCESS;
      }
      if (argument == "--input") {
        if (++index == argc) {
          throw std::runtime_error("--input requires values");
        }
        input = ParseInput(argv[index]);
      } else if (ast_path.empty()) {
        ast_path = argument;
      } else {
        throw std::runtime_error("too many positional arguments");
      }
    }

    std::istream* source = &std::cin;
    std::ifstream file;
    if (!ast_path.empty()) {
      file.open(ast_path);
      if (!file) {
        throw std::runtime_error("cannot open file: " + ast_path);
      }
      source = &file;
    }
    Json program = Json::parse(*source);
    Interpreter(std::move(input)).Run(program);
  } catch (const std::exception& error) {
    std::cerr << "Error: " << error.what() << '\n';
    return ExitCodes::ERROR;
  }
}
