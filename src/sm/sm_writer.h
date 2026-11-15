#ifndef HW1_SRC_SM_SM_WRITER_H_
#define HW1_SRC_SM_SM_WRITER_H_

#include <nlohmann/json.hpp>

#include "ir/ir.h"

nlohmann::json WriteSm(const ir::Program& program);

#endif  // HW1_SRC_SM_SM_WRITER_H_
