#include <cstdint>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "compiler/ast_compiler.h"
#include "sm/sm_loader.h"
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

std::vector<std::int64_t> ParseInput(const std::string& text) {
  std::vector<std::int64_t> values;
  std::stringstream stream(text);
  std::string part;
  while (std::getline(stream, part, ',')) {
    if (part.empty()) {
      throw std::runtime_error("empty value in --input");
    }
    std::size_t parsed_characters = 0;
    const std::int64_t value = std::stoll(part, &parsed_characters);
    if (parsed_characters != part.size()) {
      throw std::runtime_error("invalid number in --input: " + part);
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
    std::vector<std::int64_t> input;
    for (int index = 1; index < argc; ++index) {
      const std::string argument = argv[index];
      if (argument == "--help") {
        std::cout << "Usage: interpreter [program.json] [--sm] "
                     "[--input 1,2,3] [--dump-ir]\n";
        return ExitCodes::SUCCESS;
      }
      if (argument == "--dump-ir") {
        dump_ir = true;
      } else if (argument == "--sm") {
        sm = true;
      } else if (argument == "--input") {
        if (++index == argc) {
          throw std::runtime_error("--input requires values");
        }
        input = ParseInput(argv[index]);
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
    if (sm || json.is_array()) {
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
