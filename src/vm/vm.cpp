#include "vm/vm.h"

#include <limits>
#include <map>
#include <ostream>
#include <stdexcept>
#include <string>

#include "vm/value.h"

namespace {

std::int64_t Wrap32(std::int64_t value) {
  constexpr std::int64_t kModulus = std::int64_t{1} << 32;
  const std::int64_t low = static_cast<std::uint32_t>(value);
  return low > std::numeric_limits<std::int32_t>::max() ? low - kModulus : low;
}

std::int64_t Apply(ir::BinaryOp op, std::int64_t left, std::int64_t right,
                   IntegerMode mode) {
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
      if (mode == IntegerMode::kWrapping32) return Wrap32(left + right);
      if (!__builtin_add_overflow(left, right, &result)) return result;
      break;
    case Op::kSub:
      if (mode == IntegerMode::kWrapping32) return Wrap32(left - right);
      if (!__builtin_sub_overflow(left, right, &result)) return result;
      break;
    case Op::kMul:
      if (mode == IntegerMode::kWrapping32) return Wrap32(left * right);
      if (!__builtin_mul_overflow(left, right, &result)) return result;
      break;
    case Op::kDiv:
      return mode == IntegerMode::kWrapping32 ? Wrap32(left / right)
                                              : left / right;
    case Op::kRem:
      return left % right;
    case Op::kEq:
      return left == right;
    case Op::kNe:
      return left != right;
    case Op::kLt:
      return left < right;
    case Op::kLe:
      return left <= right;
    case Op::kGt:
      return left > right;
    case Op::kGe:
      return left >= right;
    case Op::kAnd:
      return left != 0 && right != 0;
    case Op::kOr:
      return left != 0 || right != 0;
  }
  throw std::runtime_error("integer overflow");
}

class Machine {
 public:
  Machine(const std::vector<std::int64_t>& input, std::ostream& output,
          IntegerMode mode)
      : input_(input), output_(output), mode_(mode) {}

  void Run(const ir::Program& program) {
    while (pc_ < program.size()) {
      const auto address = pc_++;
      try {
        std::visit([this](const auto& instruction) { Step(instruction); },
                   program[address]);
      } catch (const std::runtime_error& error) {
        throw ExecutionError(address, error.what());
      }
    }
    if (pc_ != program.size()) {
      throw std::logic_error("invalid IR at program exit");
    }
  }

 private:
  runtime::Value Pop() {
    if (stack_.empty()) throw std::runtime_error("stack underflow");
    const auto value = stack_.back();
    stack_.pop_back();
    return value;
  }

  void Step(const ir::Push& op) {
    stack_.push_back(mode_ == IntegerMode::kWrapping32 ? Wrap32(op.value)
                                                       : op.value);
  }

  void Step(const ir::Load& op) {
    const auto found = variables_.find(op.name);
    if (found != variables_.end()) {
      stack_.push_back(found->second);
    } else if (op.default_zero) {
      stack_.push_back(std::int64_t{0});
    } else {
      throw std::runtime_error("uninitialized variable: " + op.name);
    }
  }

  void Step(const ir::Store& op) { variables_[op.name] = Pop(); }

  std::int64_t ReadInput(const std::string& operation) {
    if (input_position_ == input_.size()) {
      throw std::runtime_error("not enough input values for " + operation);
    }
    const auto value = input_[input_position_++];
    if (mode_ == IntegerMode::kWrapping32 &&
        (value < std::numeric_limits<std::int32_t>::min() ||
         value > std::numeric_limits<std::int32_t>::max())) {
      throw std::runtime_error(
          "input value is outside the signed 32-bit range");
    }
    return value;
  }

  void Step(const ir::Read& op) {
    variables_[op.name] = ReadInput("read(" + op.name + ")");
  }

  void Step(const ir::ReadStack&) { stack_.push_back(ReadInput("READ")); }

  void Step(const ir::Write&) {
    heap_.Write(Pop(), output_);
    output_ << '\n';
  }

  void Step(const ir::Binary& op) {
    const auto right = runtime::Integer(Pop());
    const auto left = runtime::Integer(Pop());
    stack_.push_back(Apply(op.op, left, right, mode_));
  }

  void Step(const ir::ToBool&) {
    stack_.push_back(static_cast<std::int64_t>(runtime::Integer(Pop()) != 0));
  }

  void Step(const ir::Jump& op) { pc_ = op.target; }

  void Step(const ir::Branch& op) {
    if ((runtime::Integer(Pop()) != 0) == op.when_true) pc_ = op.target;
  }

  void Step(const ir::Array& op) {
    if (op.size > stack_.size()) throw std::runtime_error("stack underflow");
    const auto first = stack_.end() - op.size;
    const auto reference = heap_.Allocate(runtime::Array(first, stack_.end()));
    stack_.erase(first, stack_.end());
    stack_.push_back(reference);
  }

  void Step(const ir::String& op) {
    stack_.push_back(heap_.Allocate(op.bytes));
  }

  void Step(const ir::Length&) { stack_.push_back(heap_.Length(Pop())); }

  void Step(const ir::Elem&) {
    const auto index = runtime::Integer(Pop());
    stack_.push_back(heap_.Elem(Pop(), index));
  }

  void Step(const ir::SetElem&) {
    const auto element = Pop();
    const auto index = runtime::Integer(Pop());
    heap_.Set(Pop(), index, element);
  }

  const std::vector<std::int64_t>& input_;
  std::ostream& output_;
  IntegerMode mode_;
  runtime::Heap heap_;
  std::map<std::string, runtime::Value> variables_;
  std::vector<runtime::Value> stack_;
  std::size_t pc_ = 0;
  std::size_t input_position_ = 0;
};

}  // namespace

void Execute(const ir::Program& program, const std::vector<std::int64_t>& input,
             std::ostream& output, IntegerMode mode) {
  Machine(input, output, mode).Run(program);
}
