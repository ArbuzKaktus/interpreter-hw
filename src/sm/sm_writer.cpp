#include "sm/sm_writer.h"

#include <limits>
#include <set>
#include <stdexcept>
#include <string>

namespace {

using nlohmann::json;

std::string Label(std::size_t address) {
  return "L_" + std::to_string(address);
}

class Writer {
 public:
  explicit Writer(json& result) : result_(result) {}

  void operator()(const ir::Push& op) const {
    if (op.value < std::numeric_limits<std::int32_t>::min() ||
        op.value > std::numeric_limits<std::int32_t>::max()) {
      throw std::runtime_error(
          "SM constant is outside the signed 32-bit range");
    }
    result_.push_back({{"CONST", op.value}});
  }

  void operator()(const ir::Load& op) const {
    if (op.default_zero) {
      throw std::logic_error("SM cannot encode load with a default value");
    }
    result_.push_back({{"LD", op.name}});
  }

  void operator()(const ir::Store& op) const {
    result_.push_back({{"ST", op.name}});
  }

  void operator()(const ir::Read& op) const {
    (*this)(ir::ReadStack{});
    (*this)(ir::Store{op.name});
  }

  void operator()(const ir::ReadStack&) const { result_.push_back("READ"); }
  void operator()(const ir::Write&) const { result_.push_back("WRITE"); }

  void operator()(const ir::Binary& op) const {
    result_.push_back({{"BINOP", ir::Symbol(op.op)}});
  }

  void operator()(const ir::ToBool&) const {
    (*this)(ir::Push{0});
    (*this)(ir::Binary{ir::BinaryOp::kNe});
  }

  void operator()(const ir::Jump& op) const {
    result_.push_back({{"JMP", Label(op.target)}});
  }

  void operator()(const ir::Branch& op) const {
    result_.push_back({{op.when_true ? "JNZ" : "JZ", Label(op.target)}});
  }
  void operator()(const ir::Array& op) const {
    result_.push_back({{"ARRAY", op.size}});
  }
  void operator()(const ir::String& op) const {
    result_.push_back({{"STRING", op.bytes}});
  }
  void operator()(const ir::Length&) const { result_.push_back("LENGTH"); }
  void operator()(const ir::Sexp& op) const {
    result_.push_back({{"SEXP", json::array({op.tag, op.size})}});
  }
  void operator()(const ir::Elem&) const { result_.push_back("ELEM"); }
  void operator()(const ir::SetElem&) const { result_.push_back("SET"); }
  void operator()(const ir::Scope&) const { result_.push_back("SCOPE"); }
  void operator()(const ir::Enter&) const { result_.push_back("ENTER"); }
  void operator()(const ir::Leave&) const { result_.push_back("LEAVE"); }
  void operator()(const ir::MatchFailure&) const { result_.push_back("MF"); }
  void operator()(const ir::Dup&) const { result_.push_back("DUP"); }
  void operator()(const ir::Drop&) const { result_.push_back("DROP"); }
  void operator()(const ir::PatternTest& op) const {
    if (op.kind == ir::PatternKind::kTag) {
      result_.push_back(
          {{"PATT", {{"tag", {{"name", op.tag}, {"length", op.arity}}}}}});
    } else {
      result_.push_back({{"PATT", ir::PatternName(op.kind)}});
    }
  }

 private:
  json& result_;
};

}  // namespace

nlohmann::json WriteSm(const ir::Program& program) {
  std::set<std::size_t> targets;
  for (const auto& instruction : program) {
    if (const auto* jump = std::get_if<ir::Jump>(&instruction)) {
      targets.insert(jump->target);
    } else if (const auto* branch = std::get_if<ir::Branch>(&instruction)) {
      targets.insert(branch->target);
    }
  }
  if (!targets.empty() && *targets.rbegin() > program.size()) {
    throw std::logic_error("IR jump outside the program");
  }

  auto result = json::array();
  const Writer writer(result);
  for (std::size_t address = 0; address < program.size(); ++address) {
    if (targets.count(address)) result.push_back({{"LABEL", Label(address)}});
    std::visit(writer, program[address]);
  }
  if (targets.count(program.size())) {
    result.push_back({{"LABEL", Label(program.size())}});
  }
  return result;
}
