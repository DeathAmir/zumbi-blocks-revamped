#include "asset_archive.hpp"
#include "zs_compiler.hpp"
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc == 4 && std::string(argv[1]) == "compile") {
        std::string error;
        if (!zumbi::ZsCompiler::compile(argv[2], argv[3], error)) {
            std::cerr << "zsc failed: " << error << '\\n';
            return 1;
        }
        std::cout << "ZS compiled to readable ZSA assembly\\n";
        return 0;
    }
    if (argc == 5 && std::string(argv[1]) == "pack") {
        std::string error;
        if (!zumbi::AssetArchive::pack(argv[2], argv[3], argv[4], error)) {
            std::cerr << "pack failed: " << error << '\\n';
            return 1;
        }
        std::cout << "archive created\\n";
        return 0;
    }
    if (argc == 5 && std::string(argv[1]) == "unpack") {
        std::string error;
        if (!zumbi::AssetArchive::unpack(argv[2], argv[3], argv[4], error)) {
            std::cerr << "unpack failed: " << error << '\\n';
            return 1;
        }
        std::cout << "archive verified and extracted\\n";
        return 0;
    }
    std::cout << "Zumbi Native Engine 0.2.0\\n"
              << "Usage:\\n"
              << "  zumbi_native compile <input.zs> <output.zsa>\\n"
              << "  zumbi_native pack <input-file> <output.zbp> <64-char-key-hex>\\n"
              << "  zumbi_native unpack <input.zbp> <output-file> <64-char-key-hex>\\n";
    return 0;
}
