#include "vm/value.h"

#include <iomanip>
#include <ostream>
#include <stdexcept>
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
  throw std::runtime_error("expected an array or a string");
}

Reference Heap::Allocate(Object object) {
  const Reference reference{objects_.size()};
  objects_.push_back(std::move(object));
  return reference;
}

std::int64_t Heap::Length(const Value& value) const {
  const auto& object = objects_.at(GetReference(value).index);
  return std::visit([](const auto& elements) {
    return static_cast<std::int64_t>(elements.size());
  }, object);
}

Value Heap::Elem(const Value& value, std::int64_t index) const {
  const auto& object = objects_.at(GetReference(value).index);
  if (const auto* array = std::get_if<Array>(&object)) {
    return (*array)[CheckedIndex(index, array->size())];
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
  if (active[index]) throw std::runtime_error("cannot write a cyclic array");
  active[index] = true;
  output << '[';
  const auto& array = std::get<Array>(object);
  for (std::size_t i = 0; i < array.size(); ++i) {
    if (i != 0) output << ", ";
    Write(array[i], output, active);
  }
  output << ']';
  active[index] = false;
}

}  // namespace runtime
