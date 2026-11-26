#pragma once
#include <filesystem>
#include <functional>
#include <memory>
#include <unordered_map>
#include <string>
#include <variant>
#include <vector>

namespace zumbi {
struct ZsArray;
struct ZsMap;
using ZsValue = std::variant<std::monostate, bool, double, std::string,
                             std::shared_ptr<ZsArray>, std::shared_ptr<ZsMap>>;
struct ZsArray { std::vector<ZsValue> values; };
struct ZsMap { std::unordered_map<std::string, ZsValue> values; };
using ZsNativeCall = std::function<ZsValue(const std::string&, const std::vector<ZsValue>&)>;

class ZsCompiler {
public:
    static bool compile(const std::filesystem::path& input,
                        const std::filesystem::path& output,
                        std::string& error);
    static bool disassemble(const std::filesystem::path& input,
                            const std::filesystem::path& output,
                            std::string& error);
};

class ZsVm {
public:
    static bool run(const std::filesystem::path& bytecode,
                    const ZsNativeCall& nativeCall,
                    std::string& error);
};
}
