#ifndef HW1_SRC_IR_IR_H_
#define HW1_SRC_IR_IR_H_

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace ir {

enum class BinaryOp {
  kAdd,
  kSub,
  kMul,
  kDiv,
  kRem,
  kEq,
  kNe,
  kLt,
  kLe,
  kGt,
  kGe
};

std::string_view Symbol(BinaryOp op);

struct Push {
  std::int64_t value;
};
struct Load {
  std::string name;
  bool default_zero = false;
};
struct Store {
  std::string name;
};
struct Read {
  std::string name;
};
struct Write {};
struct Binary {
  BinaryOp op;
};
struct ToBool {};

// Jump targets are instruction indices. Program::size() means exit.
struct Jump {
  std::size_t target;
};
// Consumes the condition on both paths.
struct Branch {
  std::size_t target;
  bool when_true;
};

using Instruction =
    std::variant<Push, Load, Store, Read, Write, Binary, ToBool, Jump, Branch>;
using Program = std::vector<Instruction>;

void Dump(const Program& program, std::ostream& output);

}  // namespace ir

#endif  // HW1_SRC_IR_IR_H_
