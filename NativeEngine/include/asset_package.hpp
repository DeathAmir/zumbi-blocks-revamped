#pragma once
#include <filesystem>
#include <string>

namespace zumbi {
class AssetPackage {
public:
    static bool pack(const std::filesystem::path& source,
                     const std::filesystem::path& output,
                     const std::string& privateKeyHex,
                     std::string& error);
    static bool verify(const std::filesystem::path& packageRoot,
                       const std::string& trustedPublicKeyHex,
                       std::string& error);
    static bool selfTest(std::string& error);
    static bool derivePublicKey(const std::string& privateKeyHex,
                                std::string& publicKeyHex,
                                std::string& error);
};
}
