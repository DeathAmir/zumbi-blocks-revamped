#pragma once
#include <filesystem>
#include <functional>
#include <string>
#include <variant>
#include <vector>

namespace zumbi {
using ZsValue = std::variant<std::monostate, bool, double, std::string>;
using ZsNativeCall = std::function<ZsValue(const std::string&, const std::vector<ZsValue>&)>;

class ZsCompiler {
public:
    static bool compile(const std::filesystem::path& input,
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
