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
}bool loadModule(const std::filesystem::path& path,Module& m,std::string& error) {
    std::ifstream in(path,std::ios::binary); if(!in){error="Cannot open ZSC module.";return false;}
    std::array<char,8> magic{};
    std::uint64_t rawSize=0,packedSize=0;
    if(!in.read(magic.data(),magic.size())||magic!=std::array<char,8>{'Z','S','C','V','M','0','0','2'}||
       !readRaw(in,rawSize)||!readRaw(in,packedSize)||rawSize>256ull*1024*1024||packedSize>256ull*1024*1024) {
        error="Invalid ZSC signature, version, or size.";return false;
    }
    std::vector<unsigned char> packed(static_cast<std::size_t>(packedSize));
    if(packedSize && !in.read(reinterpret_cast<char*>(packed.data()),static_cast<std::streamsize>(packedSize))) {
        error="Truncated ZSC payload.";return false;
    }
    std::string raw;
    if(!rleDecompress(packed,static_cast<std::size_t>(rawSize),raw)) {error="Invalid compressed ZSC payload.";return false;}
    std::istringstream data(raw,std::ios::binary);
    std::uint32_t ns=0,nn=0,nf=0;
    if(!readRaw(data,ns)||!readRaw(data,nn)||!readRaw(data,nf)||ns>1000000||nn>1000000||nf>100000) {error="Invalid ZSC tables.";return false;}
    for(std::uint32_t i=0;i<ns;++i){std::string v;if(!readString(data,v)){error="Truncated ZSC string table.";return false;}m.strings.push_back(std::move(v));}
    for(std::uint32_t i=0;i<nn;++i){double n;if(!readRaw(data,n)){error="Truncated ZSC constants.";return false;}m.numbers.push_back(n);}
    for(std::uint32_t i=0;i<nf;++i){
        Function f; std::uint32_t params=0,locals=0,count=0;
        if(!readString(data,f.symbol)||!readRaw(data,params)||!readRaw(data,locals)||!readRaw(data,count)||params>4096||locals>65536||count>10000000){error="Invalid ZSC function.";return false;}
        f.locals=locals; f.params.resize(params);
        for(auto& p:f.params) if(!readString(data,p)){error="Truncated ZSC parameter list.";return false;}
        for(std::uint32_t j=0;j<count;++j){std::uint8_t op;Ins x;if(!readRaw(data,op)||!readRaw(data,x.a)||!readRaw(data,x.b)||op>static_cast<std::uint8_t>(Op::Return)){error="Invalid ZSC instruction.";return false;}x.op=static_cast<Op>(op);f.code.push_back(x);}
        m.functions.push_back(std::move(f));
    }
    return true;
}    std::ostringstream raw(std::ios::binary);
    const auto ns=static_cast<std::uint32_t>(strings.size());const auto nn=static_cast<std::uint32_t>(parser.numbers().size());const auto nf=static_cast<std::uint32_t>(functions.size());
    writeRaw(raw,ns);writeRaw(raw,nn);writeRaw(raw,nf);
    for(const auto& v:strings)writeString(raw,v);
    for(const auto n:parser.numbers())writeRaw(raw,n);
    for(const auto& f:functions){writeString(raw,f.symbol);const auto p=static_cast<std::uint32_t>(f.params.size());writeRaw(raw,p);writeRaw(raw,f.locals);const auto c=static_cast<std::uint32_t>(f.code.size());writeRaw(raw,c);for(const auto& ins:f.code){const auto op=static_cast<std::uint8_t>(ins.op);writeRaw(raw,op);writeRaw(raw,ins.a);writeRaw(raw,ins.b);}}
    const auto rawBytes=raw.str();
    const auto packed=rleCompress(rawBytes);
    std::ofstream out(output,std::ios::binary);if(!out){error="Cannot create ZSC output.";return false;}
    const std::array<char,8> magic{'Z','S','C','V','M','0','0','2'};out.write(magic.data(),magic.size());
    const auto rawSize=static_cast<std::uint64_t>(rawBytes.size());const auto packedSize=static_cast<std::uint64_t>(packed.size());
    writeRaw(out,rawSize);writeRaw(out,packedSize);
    if(!packed.empty())out.write(reinterpret_cast<const char*>(packed.data()),static_cast<std::streamsize>(packed.size()));
