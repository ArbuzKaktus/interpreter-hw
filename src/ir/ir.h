#ifndef HW1_SRC_IR_IR_H_
#define HW1_SRC_IR_IR_H_

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace ir {

enum class BinaryOp {
  kAdd, kSub, kMul, kDiv, kRem, kEq, kNe, kLt, kLe, kGt, kGe, kAnd, kOr
};

std::string_view Symbol(BinaryOp op);
std::optional<BinaryOp> FindBinaryOp(std::string_view symbol);
bool IsArithmetic(BinaryOp op);
bool IsComparison(BinaryOp op);

struct Push { std::int64_t value; };
struct Load {
  std::string name;
  bool default_zero = false;
};
struct Store { std::string name; };
struct Read { std::string name; };
struct ReadStack {};
struct Write {};
struct Binary { BinaryOp op; };
struct ToBool {};
struct Array { std::size_t size; };
struct String { std::string bytes; };
struct Sexp { std::string tag; std::size_t size; };
struct Length {};
struct Elem {};
struct SetElem {};
struct Scope {};
struct Enter {};
struct Leave {};
struct MatchFailure {};
enum class PatternKind { kInt, kString, kArray, kSexp, kTag };
struct PatternTest {
  PatternKind kind;
  std::string tag;
  std::size_t arity = 0;
};

// Jump targets are instruction indices. Program::size() means exit.
struct Jump { std::size_t target; };
// Consumes the condition on both paths.
struct Branch { std::size_t target; bool when_true; };

using Instruction =
    std::variant<Push, Load, Store, Read, ReadStack, Write, Binary, ToBool,
                 Jump, Branch, Array, String, Length, Elem, SetElem, Sexp,
                 Scope, Enter, Leave, MatchFailure, PatternTest>;
using Program = std::vector<Instruction>;

void Dump(const Program& program, std::ostream& output);

}  // namespace ir

#endif  // HW1_SRC_IR_IR_H_
