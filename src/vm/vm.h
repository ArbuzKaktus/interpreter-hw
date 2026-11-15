#ifndef HW1_SRC_VM_VM_H_
#define HW1_SRC_VM_VM_H_

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <stdexcept>
#include <vector>

#include "ir/ir.h"

enum class IntegerMode { kChecked64, kWrapping32 };

class ExecutionError : public std::runtime_error {
 public:
  ExecutionError(std::size_t pc, const char* message)
      : std::runtime_error(message), pc_(pc) {}

  std::size_t pc() const { return pc_; }

 private:
  std::size_t pc_;
};

void Execute(const ir::Program& program, const std::vector<std::int64_t>& input,
             std::ostream& output, IntegerMode mode = IntegerMode::kChecked64);

#endif  // HW1_SRC_VM_VM_H_
