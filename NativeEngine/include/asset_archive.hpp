#pragma once
#include <filesystem>
#include <string>

namespace zumbi {
class AssetArchive {
public:
    static bool pack(const std::filesystem::path& input,
                     const std::filesystem::path& output,
                     const std::string& key_hex,
                     std::string& error);
    static bool unpack(const std::filesystem::path& input,
                       const std::filesystem::path& output,
                       const std::string& key_hex,
                       std::string& error);
};
}
