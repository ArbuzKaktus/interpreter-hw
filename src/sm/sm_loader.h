#ifndef HW1_SRC_SM_SM_LOADER_H_
#define HW1_SRC_SM_SM_LOADER_H_

#include <cstddef>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "ir/ir.h"

struct SmProgram {
  ir::Program instructions;
  // Indexed by IR address. Values are zero-based JSON array indices.
  std::vector<std::size_t> source_indices;
};

SmProgram LoadSm(const nlohmann::json& code);

#endif  // HW1_SRC_SM_SM_LOADER_H_
