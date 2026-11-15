#ifndef HW1_SRC_VM_VALUE_H_
#define HW1_SRC_VM_VALUE_H_

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <optional>
#include <set>
#include <string>
#include <variant>
#include <vector>

#include "ir/ir.h"

namespace runtime {

struct Reference { std::size_t index; };
using Value = std::variant<std::int64_t, Reference>;
using Array = std::vector<Value>;
struct Sexp {
  std::string tag;
  Array fields;
};
using Object = std::variant<Array, std::string, Sexp>;

std::int64_t Integer(const Value& value);

// The arena owns objects. References can form cycles without ownership cycles.
class Heap {
 public:
  Reference Allocate(Object object);
  std::int64_t Length(const Value& value) const;
  Value Elem(const Value& value, std::int64_t index) const;
  void Set(const Value& value, std::int64_t index, Value element);
  void Write(const Value& value, std::ostream& output) const;
  bool Matches(const Value& value, const ir::PatternTest& pattern) const;
  std::optional<int> Compare(const Value& left, const Value& right) const;

 private:
  static Reference GetReference(const Value& value);
  void Write(const Value& value, std::ostream& output,
             std::vector<bool>& active) const;
  void WriteList(const Sexp& root, std::ostream& output,
                 std::vector<bool>& active) const;
  using Pairs = std::set<std::pair<std::size_t, std::size_t>>;
  std::optional<int> Compare(const Value& left, const Value& right,
                             Pairs& visited) const;

  std::vector<Object> objects_;
};

}  // namespace runtime

#endif  // HW1_SRC_VM_VALUE_H_
