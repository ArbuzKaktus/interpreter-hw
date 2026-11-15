#include "sm/sm_loader.h"

#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
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
  const auto symbol = RequireName(operand);
  if (const auto op = ir::FindBinaryOp(symbol)) return *op;
  throw std::runtime_error("unknown binary operator: " + symbol);
}

std::int64_t DecodeConstant(const json& operand) {
  if (!operand.is_number_integer() ||
      (operand.is_number_unsigned() &&
       operand.get<std::uint64_t>() >
           std::numeric_limits<std::int32_t>::max())) {
    throw std::runtime_error("CONST expects a signed 32-bit integer");
  }
  const auto value = operand.get<std::int64_t>();
  if (value < std::numeric_limits<std::int32_t>::min() ||
      value > std::numeric_limits<std::int32_t>::max()) {
    throw std::runtime_error("CONST expects a signed 32-bit integer");
  }
  return value;
}

std::runtime_error ErrorAt(std::size_t index, const std::string& message) {
  return std::runtime_error("SM instruction " + std::to_string(index) + ": " +
                            message);
}

}  // namespace

SmProgram LoadSm(const nlohmann::json& code) {
  if (!code.is_array()) {
    throw std::runtime_error("SM program must be a JSON array");
  }
  SmProgram result;
  auto& program = result.instructions;
  const auto emit = [&result](std::size_t index,
                              ir::Instruction instruction) {
    result.instructions.push_back(std::move(instruction));
    result.source_indices.push_back(index);
  };
  std::map<std::string, std::size_t> labels;
  std::vector<Fixup> fixups;
  for (std::size_t index = 0; index < code.size(); ++index) {
    const auto& instruction = code[index];
    try {
      if (instruction == "READ") {
        emit(index, ir::ReadStack{});
        continue;
      }
      if (instruction == "WRITE") {
        emit(index, ir::Write{});
        continue;
      }
      if (instruction == "LENGTH") {
        emit(index, ir::Length{});
        continue;
      }
      if (instruction == "ELEM") {
        emit(index, ir::Elem{});
        continue;
      }
      if (instruction == "SET") {
        emit(index, ir::SetElem{});
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
        emit(index, ir::Push{DecodeConstant(operand)});
      } else if (opcode == "ARRAY") {
        const auto size = DecodeConstant(operand);
        if (size < 0) throw std::runtime_error("negative array size");
        emit(index, ir::Array{static_cast<std::size_t>(size)});
      } else if (opcode == "STRING") {
        if (!operand.is_string()) {
          throw std::runtime_error("STRING expects a string operand");
        }
        emit(index, ir::String{operand.get<std::string>()});
      } else if (opcode == "LD") {
        emit(index, ir::Load{RequireName(operand)});
      } else if (opcode == "ST") {
        emit(index, ir::Store{RequireName(operand)});
      } else if (opcode == "BINOP") {
        emit(index, ir::Binary{DecodeBinary(operand)});
      } else if (opcode == "LABEL") {
        const auto label = RequireName(operand);
        if (!labels.emplace(label, program.size()).second) {
          throw std::runtime_error("duplicate label: " + label);
        }
        continue;
      } else if (opcode == "JMP" || opcode == "JZ" || opcode == "JNZ") {
        fixups.push_back({program.size(), index, RequireName(operand)});
        if (opcode == "JMP") {
          emit(index, ir::Jump{0});
        } else {
          emit(index, ir::Branch{0, opcode == "JNZ"});
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
  return result;
}
