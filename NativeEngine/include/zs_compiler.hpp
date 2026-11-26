#pragma once
#include <filesystem>
#include <string>

namespace zumbi {
class ZsCompiler {
public:
    static bool compile(const std::filesystem::path& input,
                        const std::filesystem::path& output,
                        std::string& error);
};
}
