#ifndef HW1_SRC_SM_SM_LOADER_H_
#define HW1_SRC_SM_SM_LOADER_H_

#include <nlohmann/json_fwd.hpp>

#include "ir/ir.h"

ir::Program LoadSm(const nlohmann::json& code);

#endif  // HW1_SRC_SM_SM_LOADER_H_
