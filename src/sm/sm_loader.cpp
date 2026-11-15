#include "sm/sm_loader.h"

#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace {

using nlohmann::json;

struct Fixup {
  std::size_t address;
  std::size_t source_index;
  std::string label;
};

std::string RequireName(const json& operand) {
  if (!operand.is_string() || operand.get_ref<const std::string&>().empty()) {
    throw std::runtime_error("expected a nonempty string operand");
  }
  return operand.get<std::string>();
}

ir::BinaryOp DecodeBinary(const json& operand) {
  using Op = ir::BinaryOp;
  constexpr Op kOperators[] = {Op::kAdd, Op::kSub, Op::kMul, Op::kDiv, Op::kRem,
                               Op::kEq,  Op::kNe,  Op::kLt,  Op::kLe,  Op::kGt,
                               Op::kGe,  Op::kAnd, Op::kOr};
  const auto symbol = RequireName(operand);
  for (const auto op : kOperators) {
    if (ir::Symbol(op) == symbol) return op;
  }
  throw std::runtime_error("unknown binary operator: " + symbol);
}

std::int64_t DecodeConstant(const json& operand) {
  if (!operand.is_number_integer() ||
      (operand.is_number_unsigned() &&
       operand.get<std::uint64_t>() >
           std::numeric_limits<std::int64_t>::max())) {
    throw std::runtime_error("CONST expects a signed 64-bit integer");
  }
  return operand.get<std::int64_t>();
}

std::runtime_error ErrorAt(std::size_t index, const std::string& message) {
  return std::runtime_error("SM instruction " + std::to_string(index) + ": " +
                            message);
}

}  // namespace

ir::Program LoadSm(const nlohmann::json& code) {
  if (!code.is_array()) {
    throw std::runtime_error("SM program must be a JSON array");
  }
  ir::Program program;
  std::map<std::string, std::size_t> labels;
  std::vector<Fixup> fixups;
  for (std::size_t index = 0; index < code.size(); ++index) {
    const auto& instruction = code[index];
    try {
      if (instruction == "READ") {
        program.emplace_back(ir::ReadStack{});
        continue;
      }
      if (instruction == "WRITE") {
        program.emplace_back(ir::Write{});
        continue;
      }
      if (!instruction.is_object() || instruction.size() != 1) {
        throw std::runtime_error(
            "expected READ, WRITE or an object with one opcode");
      }
      const auto item = instruction.begin();
      const auto& opcode = item.key();
      const auto& operand = item.value();
      if (opcode == "CONST") {
        program.emplace_back(ir::Push{DecodeConstant(operand)});
      } else if (opcode == "LD") {
        program.emplace_back(ir::Load{RequireName(operand)});
      } else if (opcode == "ST") {
        program.emplace_back(ir::Store{RequireName(operand)});
      } else if (opcode == "BINOP") {
        program.emplace_back(ir::Binary{DecodeBinary(operand)});
      } else if (opcode == "LABEL") {
        const auto label = RequireName(operand);
        if (!labels.emplace(label, program.size()).second) {
          throw std::runtime_error("duplicate label: " + label);
        }
      } else if (opcode == "JMP" || opcode == "JZ" || opcode == "JNZ") {
        fixups.push_back({program.size(), index, RequireName(operand)});
        if (opcode == "JMP") {
          program.emplace_back(ir::Jump{0});
        } else {
          program.emplace_back(ir::Branch{0, opcode == "JNZ"});
        }
      } else {
        throw std::runtime_error("unsupported opcode: " + opcode);
      }
    } catch (const std::runtime_error& error) {
      throw ErrorAt(index, error.what());
    }
  }

  for (const auto& fixup : fixups) {
    const auto target = labels.find(fixup.label);
    if (target == labels.end()) {
      throw ErrorAt(fixup.source_index, "unknown label: " + fixup.label);
    }
    auto& instruction = program[fixup.address];
    if (auto* jump = std::get_if<ir::Jump>(&instruction)) {
      jump->target = target->second;
    } else {
      std::get<ir::Branch>(instruction).target = target->second;
    }
  }
  return program;
}
