#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "compiler/ast_compiler.h"
#include "sm/sm_loader.h"
#include "sm/sm_writer.h"
#include "vm/vm.h"

namespace {

nlohmann::json ParseProgram(std::istream& source) {
  using Event = nlohmann::json::parse_event_t;
  std::vector<std::set<std::string>> object_keys;
  return nlohmann::json::parse(
      source, [&object_keys](int, Event event, nlohmann::json& value) {
        if (event == Event::object_start) {
          object_keys.emplace_back();
        } else if (event == Event::object_end) {
          object_keys.pop_back();
        } else if (event == Event::key) {
          const auto& key = value.get_ref<const std::string&>();
          if (!object_keys.back().insert(key).second) {
            throw std::runtime_error("duplicate JSON key: " + key);
          }
        }
        return true;
      });
}

void RunSm(const SmProgram& program, const std::vector<std::int64_t>& input) {
  std::ostringstream output;
  try {
    Execute(program.instructions, input, output, IntegerMode::kWrapping32);
  } catch (const ExecutionError& error) {
    throw std::runtime_error(
        "SM instruction " +
        std::to_string(program.source_indices.at(error.pc())) + ": " +
        error.what());
  }
  std::cout << output.str();
}

std::vector<std::int64_t> ParseInput(const std::string& text, bool sm) {
  constexpr char kWhitespace[] = " \t\r\n";
  if (sm && !text.empty()) {
    const auto last = text.find_last_not_of(kWhitespace);
    if (last == std::string::npos || text[last] == ',') {
      throw std::runtime_error("empty value in --input");
    }
  }
  std::vector<std::int64_t> values;
  std::stringstream stream(text);
  std::string part;
  while (std::getline(stream, part, ',')) {
    if (sm) {
      const auto first = part.find_first_not_of(kWhitespace);
      if (first == std::string::npos) {
        throw std::runtime_error("empty value in --input");
      }
      const auto last = part.find_last_not_of(kWhitespace);
      part = part.substr(first, last - first + 1);
      if (part.front() == '+') {
        throw std::runtime_error("invalid number in --input: " + part);
      }
    }
    if (part.empty()) {
      throw std::runtime_error("empty value in --input");
    }
    std::size_t parsed_characters = 0;
    const std::int64_t value = std::stoll(part, &parsed_characters);
    if (parsed_characters != part.size()) {
      throw std::runtime_error("invalid number in --input: " + part);
    }
    if (sm && (value < std::numeric_limits<std::int32_t>::min() ||
               value > std::numeric_limits<std::int32_t>::max())) {
      throw std::runtime_error("input value is outside the signed 32-bit range");
    }
    values.push_back(value);
  }
  return values;
}

enum ExitCodes { SUCCESS, FAIL };

}  // namespace

int main(int argc, char* argv[]) {
  try {
    std::string program_path;
    bool sm = false;
    bool dump_ir = false;
    bool emit_sm = false;
    std::string input_text;
    for (int index = 1; index < argc; ++index) {
      const std::string argument = argv[index];
      if (argument == "--help") {
        std::cout << "Usage: interpreter [program.json] [--sm] "
                     "[--input 1,2,3] [--dump-ir | --emit-sm]\n";
        return ExitCodes::SUCCESS;
      }
      if (argument == "--dump-ir") {
        dump_ir = true;
      } else if (argument == "--emit-sm") {
        emit_sm = true;
      } else if (argument == "--sm") {
        sm = true;
      } else if (argument == "--input") {
        if (++index == argc) {
          throw std::runtime_error("--input requires values");
        }
        input_text = argv[index];
      } else if (program_path.empty()) {
        program_path = argument;
      } else {
        throw std::runtime_error("too many positional arguments");
      }
    }

    std::istream* source = &std::cin;
    std::ifstream file;
    if (!program_path.empty()) {
      file.open(program_path);
      if (!file) {
        throw std::runtime_error("cannot open file: " + program_path);
      }
      source = &file;
    }
    const auto json = ParseProgram(*source);
    const bool is_sm = sm || json.is_array();
    if (emit_sm) {
      if (is_sm || dump_ir) {
        throw std::runtime_error("--emit-sm requires AST input and no --dump-ir");
      }
      std::cout << WriteSm(CompileAst(json, AstTarget::kStackMachine)).dump(2)
                << '\n';
      return ExitCodes::SUCCESS;
    }
    const auto input = ParseInput(input_text, is_sm);
    if (is_sm) {
      const auto program = LoadSm(json);
      if (dump_ir) {
        ir::Dump(program.instructions, std::cout);
      } else {
        RunSm(program, input);
      }
    } else {
      const auto program = CompileAst(json);
      if (dump_ir) {
        ir::Dump(program, std::cout);
      } else {
        Execute(program, input, std::cout);
      }
    }
  } catch (const std::exception& error) {
    std::cerr << "Error: " << error.what() << '\n';
    return ExitCodes::FAIL;
  }
}
