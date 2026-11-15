#ifndef HW1_SRC_VM_VM_H_
#define HW1_SRC_VM_VM_H_

#include <cstdint>
#include <iosfwd>
#include <vector>

#include "ir/ir.h"

void Execute(const ir::Program& program,
             const std::vector<std::int64_t>& input, std::ostream& output);

#endif  // HW1_SRC_VM_VM_H_
