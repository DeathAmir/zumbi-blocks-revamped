#include "asset_archive.hpp"
#include "zs_compiler.hpp"
#include <iostream>
#include <string>
#include <variant>

namespace {
std::string display(const zumbi::ZsValue& value) {
    if (const auto p=std::get_if<std::string>(&value)) return *p;
    if (const auto p=std::get_if<double>(&value)) return std::to_string(*p);
    if (const auto p=std::get_if<bool>(&value)) return *p ? "true" : "false";
    return "null";
}
}

int main(int argc, char** argv) {
    if (argc == 4 && std::string(argv[1]) == "compile") {
        std::string error;
        if (!zumbi::ZsCompiler::compile(argv[2], argv[3], error)) {
            std::cerr << "zsc failed: " << error << '\n';
            return 1;
        }
        std::cout << "ZS compiled to binary ZSC bytecode\n";
        return 0;
    }
    if (argc == 4 && std::string(argv[1]) == "disasm") {
        std::string error;
        if (!zumbi::ZsCompiler::disassemble(argv[2], argv[3], error)) {
            std::cerr << "zsa failed: " << error << '\n';
            return 1;
        }
        std::cout << "ZSC disassembled to stripped ZSA listing\n";
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "run") {
        std::string error;
        const zumbi::ZsNativeCall native = [](const std::string& name, const std::vector<zumbi::ZsValue>& args) -> zumbi::ZsValue {
            if (name == "log" || name == "print") {
                for (std::size_t i=0;i<args.size();++i) {
                    if (i) std::cout << ' ';
                    std::cout << display(args[i]);
                }
                std::cout << '\n';
                return std::monostate{};
            }
            std::cerr << "Unregistered native API: " << name << '\n';
            return std::monostate{};
        };
        if (!zumbi::ZsVm::run(argv[2], native, error)) {
            std::cerr << "zsvm failed: " << error << '\n';
            return 1;
        }
        return 0;
    }
    if (argc == 5 && std::string(argv[1]) == "pack") {
        std::string error;
        if (!zumbi::AssetArchive::pack(argv[2], argv[3], argv[4], error)) {
            std::cerr << "pack failed: " << error << '\n';
            return 1;
        }
        std::cout << "archive created\n";
        return 0;
    }
    if (argc == 5 && std::string(argv[1]) == "unpack") {
        std::string error;
        if (!zumbi::AssetArchive::unpack(argv[2], argv[3], argv[4], error)) {
            std::cerr << "unpack failed: " << error << '\n';
            return 1;
        }
        std::cout << "archive verified and extracted\n";
        return 0;
    }
    std::cout << "Zumbi Native Engine 0.4.0\n"
              << "Usage:\n"
              << "  zumbi_native compile <input.zs> <output.zsc>\n"
              << "  zumbi_native run <module.zsc>\n"
              << "  zumbi_native disasm <module.zsc> <output.zsa>\n"
              << "  zumbi_native pack <input-file> <output.zbp> <64-char-key-hex>\n"
              << "  zumbi_native unpack <input.zbp> <output-file> <64-char-key-hex>\n";
    return 0;
}
