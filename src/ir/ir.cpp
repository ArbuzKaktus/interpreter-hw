#include "ir/ir.h"

#include <iomanip>
#include <ostream>
#include <stdexcept>

namespace ir {
namespace {

struct Operator {
  BinaryOp op;
  std::string_view symbol;
  bool arithmetic = false;
};

constexpr Operator kOperators[] = {
    {BinaryOp::kAdd, "+", true}, {BinaryOp::kSub, "-", true},
    {BinaryOp::kMul, "*", true}, {BinaryOp::kDiv, "/", true},
    {BinaryOp::kRem, "%", true}, {BinaryOp::kEq, "=="},
    {BinaryOp::kNe, "!="},       {BinaryOp::kLt, "<"},
    {BinaryOp::kLe, "<="},       {BinaryOp::kGt, ">"},
    {BinaryOp::kGe, ">="},       {BinaryOp::kAnd, "&&"},
    {BinaryOp::kOr, "!!"},
};

const Operator& Describe(BinaryOp op) {
  for (const auto& entry : kOperators) {
    if (entry.op == op) return entry;
  }
  throw std::logic_error("invalid binary opcode");
}

class Printer {
 public:
  explicit Printer(std::ostream& output) : output_(output) {}

  void operator()(const Push& op) const { output_ << "push " << op.value; }
  void operator()(const Load& op) const {
    output_ << (op.default_zero ? "load_or_zero " : "load ")
            << std::quoted(op.name);
  }
  void operator()(const Store& op) const {
    output_ << "store " << std::quoted(op.name);
  }
  void operator()(const Read& op) const {
    output_ << "read " << std::quoted(op.name);
  }
  void operator()(const ReadStack&) const { output_ << "read"; }
  void operator()(const Write&) const { output_ << "write"; }
  void operator()(const Binary& op) const {
    output_ << "binary " << Symbol(op.op);
  }
  void operator()(const ToBool&) const { output_ << "to_bool"; }
  void operator()(const Jump& op) const { output_ << "jump " << op.target; }
  void operator()(const Branch& op) const {
    output_ << (op.when_true ? "branch_true " : "branch_false ") << op.target;
  }

 private:
  std::ostream& output_;
};

}  // namespace

std::string_view Symbol(BinaryOp op) { return Describe(op).symbol; }

std::optional<BinaryOp> FindBinaryOp(std::string_view symbol) {
  for (const auto& entry : kOperators) {
    if (entry.symbol == symbol) return entry.op;
  }
  return std::nullopt;
}

bool IsArithmetic(BinaryOp op) { return Describe(op).arithmetic; }

void Dump(const Program& program, std::ostream& output) {
  for (std::size_t pc = 0; pc < program.size(); ++pc) {
    output << pc << ": ";
    std::visit(Printer(output), program[pc]);
    output << '\n';
  }
  output << program.size() << ": end\n";
}

}  // namespace ir
