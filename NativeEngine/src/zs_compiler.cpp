#include "zs_compiler.hpp"
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <functional>
#include <random>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace zumbi {
namespace {
struct Function {
    std::string name;
    std::string symbol;
    std::vector<std::string> instructions;
};

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

bool identifier(const std::string& value) {
    if (value.empty() || !(std::isalpha(static_cast<unsigned char>(value[0])) || value[0] == '_')) return false;
    for (const unsigned char c : value)
        if (!(std::isalnum(c) || c == '_')) return false;
    return true;
}

bool parse_arguments(const std::string& text, std::vector<std::string>& args) {
    std::string current;
    bool quoted = false, escaped = false;
    for (const char c : text) {
        if (escaped) { current += c; escaped = false; continue; }
        if (quoted && c == '\\') { current += c; escaped = true; continue; }
        if (c == '"') { quoted = !quoted; current += c; continue; }
        if (c == ',' && !quoted) { args.push_back(trim(current)); current.clear(); }
        else current += c;
    }
    if (quoted || escaped) return false;
    if (!trim(current).empty() || !args.empty()) args.push_back(trim(current));
    return true;
}

bool is_string_literal(const std::string& value) {
    return value.size() >= 2 && value.front() == '"' && value.back() == '"';
}

std::string make_symbol(std::mt19937_64& random, std::unordered_map<std::string, bool>& used) {
    static constexpr char alphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    std::uniform_int_distribution<int> pick(0, 35);
    for (;;) {
        std::string symbol(3, 'a');
        for (char& c : symbol) c = alphabet[pick(random)];
        if (used.emplace(symbol, true).second) return symbol;
    }
}
}

bool ZsCompiler::compile(const std::filesystem::path& input,
                         const std::filesystem::path& output,
                         std::string& error) {
    std::ifstream file(input);
    if (!file) { error = "Cannot open ZS source file."; return false; }

    std::vector<Function> functions;
    Function* active = nullptr;
    std::string line;
    std::size_t lineNumber = 0;

    while (std::getline(file, line)) {
        ++lineNumber;
        const auto code = trim(line);
        if (code.empty() || code.rfind("//", 0) == 0) continue;

        if (!active) {
            if (code.rfind("fn ", 0) != 0 || code.back() != '{') {
                error = "Line " + std::to_string(lineNumber) + ": expected fn Name() {";
                return false;
            }
            const auto open = code.find('(');
            const auto close = code.find(')', open == std::string::npos ? 0 : open);
            if (open == std::string::npos || close == std::string::npos ||
                trim(code.substr(close + 1)) != "{") {
                error = "Line " + std::to_string(lineNumber) + ": invalid function declaration.";
                return false;
            }
            const auto name = trim(code.substr(3, open - 3));
            if (!identifier(name) || !trim(code.substr(open + 1, close - open - 1)).empty()) {
                error = "Line " + std::to_string(lineNumber) + ": function names must be identifiers with no parameters in this compiler version.";
                return false;
            }
            for (const auto& function : functions)
                if (function.name == name) {
                    error = "Line " + std::to_string(lineNumber) + ": duplicate function '" + name + "'.";
                    return false;
                }
            functions.push_back(Function{name, {}, {}});
            active = &functions.back();
            continue;
        }

        if (code == "}") { active = nullptr; continue; }
        if (code.rfind("ui.", 0) != 0) {
            error = "Line " + std::to_string(lineNumber) + ": only UI instructions are supported in ZS compiler v0.1.";
            return false;
        }
        const auto open = code.find('(');
        const auto close = code.rfind(')');
        if (open == std::string::npos || close != code.size() - 1 || close < open) {
            error = "Line " + std::to_string(lineNumber) + ": invalid UI instruction.";
            return false;
        }
        const auto operation = code.substr(3, open - 3);
        std::vector<std::string> args;
        if (!parse_arguments(code.substr(open + 1, close - open - 1), args)) {
            error = "Line " + std::to_string(lineNumber) + ": malformed string or argument list.";
            return false;
        }

        std::ostringstream instruction;
        if (operation == "panel" && args.size() == 1 && is_string_literal(args[0])) {
            instruction << "  UI_PANEL " << args[0];
        } else if ((operation == "text" || operation == "button") &&
                   args.size() == 2 && is_string_literal(args[0]) && is_string_literal(args[1])) {
            instruction << (operation == "text" ? "  UI_TEXT " : "  UI_BUTTON ")
                        << args[0] << " " << args[1];
        } else if (operation == "visible" && args.size() == 2 &&
                   is_string_literal(args[0]) && (args[1] == "true" || args[1] == "false")) {
            instruction << "  UI_VISIBLE " << args[0] << " " << (args[1] == "true" ? "1" : "0");
        } else {
            error = "Line " + std::to_string(lineNumber) + ": unsupported UI call or invalid arguments.";
            return false;
        }
        active->instructions.push_back(instruction.str());
    }

    if (active) { error = "Unexpected end of file: missing '}'."; return false; }
    if (functions.empty()) { error = "Source contains no functions."; return false; }

    std::random_device device;
    const auto seed = static_cast<std::uint64_t>(device()) ^
        static_cast<std::uint64_t>(std::hash<std::string>{}(input.filename().string()));
    std::mt19937_64 random(seed);
    std::unordered_map<std::string, bool> used;
    for (auto& function : functions) {
        function.symbol = make_symbol(random, used);
    }

    std::ofstream outputFile(output, std::ios::binary);
    if (!outputFile) { error = "Cannot create ZSA output file."; return false; }
    outputFile << "; Zumbi Script Assembly v0.1\n";
    outputFile << ".module \"" << input.stem().string() << "\"\n";
    for (const auto& function : functions)
        outputFile << ".symbol " << function.name << " " << function.symbol << "\n";
    outputFile << "\n";
    for (const auto& function : functions) {
        outputFile << ".function " << function.symbol << "\n";
        for (const auto& instruction : function.instructions) outputFile << instruction << "\n";
        outputFile << ".end\n\n";
    }
    if (!outputFile) { error = "Failed while writing ZSA output."; return false; }
    return true;
}
}
