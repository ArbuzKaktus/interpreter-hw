#include "vm/value.h"

#include <iomanip>
#include <ostream>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace runtime {
namespace {

std::size_t CheckedIndex(std::int64_t index, std::size_t size) {
  if (index < 0 || static_cast<std::uint64_t>(index) >= size) {
    throw std::runtime_error("index " + std::to_string(index) +
                             " is out of bounds [0, " +
                             std::to_string(size) + ")");
  }
  return static_cast<std::size_t>(index);
}

}  // namespace

std::int64_t Integer(const Value& value) {
  if (const auto* number = std::get_if<std::int64_t>(&value)) return *number;
  throw std::runtime_error("expected an integer");
}

Reference Heap::GetReference(const Value& value) {
  if (const auto* reference = std::get_if<Reference>(&value)) return *reference;
  throw std::runtime_error("expected an array, string or S-expression");
}

Reference Heap::Allocate(Object object) {
  const Reference reference{objects_.size()};
  objects_.push_back(std::move(object));
  return reference;
}

std::int64_t Heap::Length(const Value& value) const {
  const auto& object = objects_.at(GetReference(value).index);
  return std::visit([](const auto& elements) {
    using T = std::decay_t<decltype(elements)>;
    if constexpr (std::is_same_v<T, Sexp>) {
      return static_cast<std::int64_t>(elements.fields.size());
    } else {
      return static_cast<std::int64_t>(elements.size());
    }
  }, object);
}

Value Heap::Elem(const Value& value, std::int64_t index) const {
  const auto& object = objects_.at(GetReference(value).index);
  if (const auto* array = std::get_if<Array>(&object)) {
    return (*array)[CheckedIndex(index, array->size())];
  }
  if (const auto* sexp = std::get_if<Sexp>(&object)) {
    return sexp->fields[CheckedIndex(index, sexp->fields.size())];
  }
  const auto& bytes = std::get<std::string>(object);
  return static_cast<std::int64_t>(
      static_cast<unsigned char>(bytes[CheckedIndex(index, bytes.size())]));
}

void Heap::Set(const Value& value, std::int64_t index, Value element) {
  auto& object = objects_.at(GetReference(value).index);
  if (auto* array = std::get_if<Array>(&object)) {
    (*array)[CheckedIndex(index, array->size())] = std::move(element);
    return;
  }
  if (auto* sexp = std::get_if<Sexp>(&object)) {
    sexp->fields[CheckedIndex(index, sexp->fields.size())] = std::move(element);
    return;
  }
  auto& bytes = std::get<std::string>(object);
  const auto position = CheckedIndex(index, bytes.size());
  const auto byte = Integer(element);
  if (byte < 0 || byte > 255) {
    throw std::runtime_error("string element is outside the byte range");
  }
  bytes[position] = static_cast<char>(static_cast<unsigned char>(byte));
}

void Heap::Write(const Value& value, std::ostream& output) const {
  std::vector<bool> active(objects_.size());
  Write(value, output, active);
}

void Heap::Write(const Value& value, std::ostream& output,
                 std::vector<bool>& active) const {
  if (const auto* number = std::get_if<std::int64_t>(&value)) {
    output << *number;
    return;
  }
  const auto index = GetReference(value).index;
  const auto& object = objects_.at(index);
  if (const auto* bytes = std::get_if<std::string>(&object)) {
    output << std::quoted(*bytes);
    return;
  }
  if (active[index]) throw std::runtime_error("cannot write a cyclic value");
  active[index] = true;
  if (const auto* sexp = std::get_if<Sexp>(&object)) {
    if (sexp->tag == "cons" || sexp->tag == "nil") {
      WriteList(*sexp, output, active);
    } else {
      output << sexp->tag;
      if (!sexp->fields.empty()) output << " (";
      for (std::size_t i = 0; i < sexp->fields.size(); ++i) {
        if (i != 0) output << ", ";
        Write(sexp->fields[i], output, active);
      }
      if (!sexp->fields.empty()) output << ')';
    }
  } else {
    output << '[';
    const auto& array = std::get<Array>(object);
    for (std::size_t i = 0; i < array.size(); ++i) {
      if (i != 0) output << ", ";
      Write(array[i], output, active);
    }
    output << ']';
  }
  active[index] = false;
}

void Heap::WriteList(const Sexp& root, std::ostream& output,
                     std::vector<bool>& active) const {
  output << '{';
  const auto* node = &root;
  std::vector<std::size_t> activated;
  bool first = true;
  while (node && node->tag == "cons" && node->fields.size() == 2) {
    if (!first) output << ", ";
    first = false;
    Write(node->fields[0], output, active);
    const auto* tail = std::get_if<Reference>(&node->fields[1]);
    if (!tail) throw std::runtime_error("invalid list tail");
    if (active[tail->index]) throw std::runtime_error("cannot write a cyclic value");
    active[tail->index] = true;
    activated.push_back(tail->index);
    node = std::get_if<Sexp>(&objects_.at(tail->index));
  }
  if (!node || node->tag != "nil" || !node->fields.empty()) {
    throw std::runtime_error("invalid list tail");
  }
  for (const auto index : activated) active[index] = false;
  output << '}';
}

bool Heap::Matches(const Value& value, const ir::PatternTest& pattern) const {
  using Kind = ir::PatternKind;
  const auto* reference = std::get_if<Reference>(&value);
  if (!reference) return pattern.kind == Kind::kInt;
  const auto& object = objects_.at(reference->index);
  switch (pattern.kind) {
    case Kind::kInt: return false;
    case Kind::kString: return std::holds_alternative<std::string>(object);
    case Kind::kArray: return std::holds_alternative<Array>(object);
    case Kind::kSexp: return std::holds_alternative<Sexp>(object);
    case Kind::kTag: {
      const auto* sexp = std::get_if<Sexp>(&object);
      return sexp && sexp->tag == pattern.tag &&
             sexp->fields.size() == pattern.arity;
    }
  }
  return false;
}

std::optional<int> Heap::Compare(const Value& left, const Value& right) const {
  Pairs visited;
  return Compare(left, right, visited);
}

std::optional<int> Heap::Compare(const Value& left, const Value& right,
                                 Pairs& visited) const {
  if (left.index() != right.index()) return std::nullopt;
  if (const auto* number = std::get_if<std::int64_t>(&left)) {
    const auto other = std::get<std::int64_t>(right);
    return *number < other ? -1 : *number > other ? 1 : 0;
  }
  const auto a = GetReference(left).index;
  const auto b = GetReference(right).index;
  const auto& x = objects_.at(a);
  const auto& y = objects_.at(b);
  if (x.index() != y.index()) return std::nullopt;
  if (const auto* bytes = std::get_if<std::string>(&x)) {
    return bytes->compare(std::get<std::string>(y));
  }
  if (a == b || !visited.emplace(a, b).second) return 0;
  const auto* xs = std::get_if<Sexp>(&x);
  const auto* ys = std::get_if<Sexp>(&y);
  if (xs && xs->tag != ys->tag) return xs->tag.compare(ys->tag);
  const auto& first = xs ? xs->fields : std::get<Array>(x);
  const auto& second = ys ? ys->fields : std::get<Array>(y);
  if (first.size() != second.size()) return first.size() < second.size() ? -1 : 1;
  for (std::size_t i = 0; i < first.size(); ++i) {
    const auto result = Compare(first[i], second[i], visited);
    if (!result || *result != 0) return result;
  }
  return 0;
}

}  // namespace runtime
