#include "sm/sm_loader.h"

#include <cstdint>
#include <limits>
#include <map>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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

ir::PatternTest DecodePattern(const json& operand) {
  if (operand.is_string()) {
    if (const auto kind = ir::FindPatternKind(operand.get<std::string>())) {
      return {*kind, {}};
    }
  } else if (operand.is_object() && operand.size() == 1 &&
             operand.contains("tag")) {
    const auto& tag = operand.at("tag");
    if (tag.is_object() && tag.size() == 2 && tag.contains("name") &&
        tag.contains("length")) {
      const auto arity = DecodeConstant(tag.at("length"));
      if (arity < 0) throw std::runtime_error("negative pattern arity");
      return {ir::PatternKind::kTag, RequireName(tag.at("name")),
              static_cast<std::size_t>(arity)};
    }
  }
  throw std::runtime_error("invalid PATT operand");
}

ir::Instruction DecodeSimple(std::string_view opcode) {
  if (opcode == "READ") return ir::ReadStack{};
  if (opcode == "WRITE") return ir::Write{};
  if (opcode == "LENGTH") return ir::Length{};
  if (opcode == "ELEM") return ir::Elem{};
  if (opcode == "SET") return ir::SetElem{};
  if (opcode == "DUP") return ir::Dup{};
  if (opcode == "DROP") return ir::Drop{};
  if (opcode == "SCOPE") return ir::Scope{};
  if (opcode == "ENTER") return ir::Enter{};
  if (opcode == "LEAVE") return ir::Leave{};
  if (opcode == "MF") return ir::MatchFailure{};
  throw std::runtime_error("unsupported opcode: " + std::string(opcode));
}

}  // namespace

SmProgram LoadSm(const nlohmann::json& code) {
  if (!code.is_array()) {
    throw std::runtime_error("SM program must be a JSON array");
  }
  SmProgram result;
  auto& program = result.instructions;
  const auto emit = [&result](std::size_t index, ir::Instruction instruction) {
    result.instructions.push_back(std::move(instruction));
    result.source_indices.push_back(index);
  };
  std::map<std::string, std::size_t> labels;
  std::vector<Fixup> fixups;
  for (std::size_t index = 0; index < code.size(); ++index) {
    const auto& instruction = code[index];
    try {
      if (instruction.is_string()) {
        emit(index, DecodeSimple(instruction.get_ref<const std::string&>()));
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
      } else if (opcode == "SEXP") {
        if (!operand.is_array() || operand.size() != 2) {
          throw std::runtime_error("SEXP expects [tag, arity]");
        }
        const auto arity = DecodeConstant(operand[1]);
        if (arity < 0) throw std::runtime_error("negative SEXP arity");
        emit(index, ir::Sexp{RequireName(operand[0]),
                             static_cast<std::size_t>(arity)});
      } else if (opcode == "LD") {
        emit(index, ir::Load{RequireName(operand)});
      } else if (opcode == "ST") {
        emit(index, ir::Store{RequireName(operand)});
      } else if (opcode == "PATT") {
        emit(index, DecodePattern(operand));
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
