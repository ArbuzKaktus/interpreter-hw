#include "ir/ir.h"

#include <iomanip>
#include <ostream>
#include <stdexcept>

namespace ir {
namespace {

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

std::string_view Symbol(BinaryOp op) {
  switch (op) {
    case BinaryOp::kAdd:
      return "+";
    case BinaryOp::kSub:
      return "-";
    case BinaryOp::kMul:
      return "*";
    case BinaryOp::kDiv:
      return "/";
    case BinaryOp::kRem:
      return "%";
    case BinaryOp::kEq:
      return "==";
    case BinaryOp::kNe:
      return "!=";
    case BinaryOp::kLt:
      return "<";
    case BinaryOp::kLe:
      return "<=";
    case BinaryOp::kGt:
      return ">";
    case BinaryOp::kGe:
      return ">=";
  }
  throw std::logic_error("invalid binary opcode");
}

void Dump(const Program& program, std::ostream& output) {
  for (std::size_t pc = 0; pc < program.size(); ++pc) {
    output << pc << ": ";
    std::visit(Printer(output), program[pc]);
    output << '\n';
  }
  output << program.size() << ": end\n";
}

}  // namespace ir
