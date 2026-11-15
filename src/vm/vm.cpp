#include "vm/vm.h"

#include <limits>
#include <map>
#include <ostream>
#include <stdexcept>
#include <string>

namespace {

std::int64_t Apply(ir::BinaryOp op, std::int64_t left, std::int64_t right) {
  using Op = ir::BinaryOp;
  if (op == Op::kDiv || op == Op::kRem) {
    if (right == 0) {
      throw std::runtime_error(op == Op::kDiv ? "division by zero"
                                              : "remainder by zero");
    }
    if (left == std::numeric_limits<std::int64_t>::min() && right == -1) {
      if (op == Op::kRem) return 0;
      throw std::runtime_error("integer overflow");
    }
  }

  std::int64_t result;
  switch (op) {
    case Op::kAdd:
      if (!__builtin_add_overflow(left, right, &result)) return result;
      break;
    case Op::kSub:
      if (!__builtin_sub_overflow(left, right, &result)) return result;
      break;
    case Op::kMul:
      if (!__builtin_mul_overflow(left, right, &result)) return result;
      break;
    case Op::kDiv: return left / right;
    case Op::kRem: return left % right;
    case Op::kEq: return left == right;
    case Op::kNe: return left != right;
    case Op::kLt: return left < right;
    case Op::kLe: return left <= right;
    case Op::kGt: return left > right;
    case Op::kGe: return left >= right;
  }
  throw std::runtime_error("integer overflow");
}

class Machine {
 public:
  Machine(const std::vector<std::int64_t>& input, std::ostream& output)
      : input_(input), output_(output) {}

  void Run(const ir::Program& program) {
    while (pc_ < program.size()) {
      std::visit([this](const auto& instruction) { Step(instruction); },
                 program[pc_++]);
    }
    if (pc_ != program.size() || !stack_.empty()) {
      throw std::logic_error("invalid IR at program exit");
    }
  }

 private:
  std::int64_t Pop() {
    if (stack_.empty()) throw std::logic_error("IR stack underflow");
    const auto value = stack_.back();
    stack_.pop_back();
    return value;
  }

  void Step(const ir::Push& op) { stack_.push_back(op.value); }

  void Step(const ir::Load& op) {
    const auto found = variables_.find(op.name);
    if (found != variables_.end()) {
      stack_.push_back(found->second);
    } else if (op.default_zero) {
      stack_.push_back(0);
    } else {
      throw std::runtime_error("uninitialized variable: " + op.name);
    }
  }

  void Step(const ir::Store& op) { variables_[op.name] = Pop(); }

  void Step(const ir::Read& op) {
    if (input_position_ == input_.size()) {
      throw std::runtime_error("not enough input values for read(" + op.name +
                               ")");
    }
    variables_[op.name] = input_[input_position_++];
  }

  void Step(const ir::Write&) { output_ << Pop() << '\n'; }

  void Step(const ir::Binary& op) {
    const auto right = Pop();
    const auto left = Pop();
    stack_.push_back(Apply(op.op, left, right));
  }

  void Step(const ir::ToBool&) {
    const bool value = Pop() != 0;
    stack_.push_back(value);
  }

  void Step(const ir::Jump& op) { pc_ = op.target; }

  void Step(const ir::Branch& op) {
    if ((Pop() != 0) == op.when_true) pc_ = op.target;
  }

  const std::vector<std::int64_t>& input_;
  std::ostream& output_;
  std::map<std::string, std::int64_t> variables_;
  std::vector<std::int64_t> stack_;
  std::size_t pc_ = 0;
  std::size_t input_position_ = 0;
};

}  // namespace

void Execute(const ir::Program& program, const std::vector<std::int64_t>& input,
             std::ostream& output) {
  Machine(input, output).Run(program);
}
