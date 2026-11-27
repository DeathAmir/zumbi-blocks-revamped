#include "asset_archive.hpp"
#include "asset_package.hpp"
#include "engine_core.hpp"
#include "game_runtime.hpp"
#include "zs_compiler.hpp"
#include <iostream>
#include <cmath>
#include <string>
#include <variant>
#include <unordered_map>
#include <cstdlib>

std::string environmentVariable(const char* name) {
#ifdef _MSC_VER
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, name) != 0 || !value) return {};
    std::string result(value);
    std::free(value);
    return result;
#else
    const char* value = std::getenv(name);
    return value ? std::string(value) : std::string{};
#endif
}

namespace {
std::string display(const zumbi::ZsValue& value) {
    if (const auto p=std::get_if<std::string>(&value)) return *p;
    if (const auto p=std::get_if<double>(&value)) return std::to_string(*p);
    if (const auto p=std::get_if<bool>(&value)) return *p ? "true" : "false";
    if (std::holds_alternative<std::shared_ptr<zumbi::ZsArray>>(value)) return "[array]";
    if (std::holds_alternative<std::shared_ptr<zumbi::ZsMap>>(value)) return "[map]";
    return "null";
}
}

int main(int argc, char** argv) {
    if (argc == 3 && std::string(argv[1]) == "game-run") {
        zumbi::NativeGameRuntime runtime;
        std::string error;
        if (!runtime.run(argv[2], error)) {
            std::cerr << "game runtime failed: " << error << '\n';
            return 1;
        }
        std::cout << "ZS gameplay module executed in native engine runtime\n";
        return 0;
    }
    if (argc == 4 && std::string(argv[1]) == "game-loop") {
        char* end = nullptr;
        const auto frames = std::strtoul(argv[3], &end, 10);
        if (!end || *end != '\0' || frames == 0 || frames > 1000000) {
            std::cerr << "game-loop frame count must be between 1 and 1000000\n";
            return 2;
        }
        zumbi::NativeGameRuntime runtime;
        std::string error;
        for (unsigned long frame = 0; frame < frames; ++frame) {
            runtime.setInput("deltaTime", 1.0 / 60.0);
            if (!runtime.run(argv[2], error)) {
                std::cerr << "game runtime failed on frame " << frame << ": " << error << '\n';
                return 1;
            }
        }
        std::cout << "Native ZS runtime completed " << frames << " persistent frames; players=" << runtime.playerCount() << '\n';
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "game-selftest") {
        zumbi::NativeGameRuntime runtime;
        runtime.setInput("key.W", true);
        runtime.setInput("key.LeftShift", true);
        runtime.setInput("testMode", true);
        runtime.setInput("deltaTime", 0.1);
        std::string error;
        if (!runtime.run("NativeEngine/scripts/player_system.zsc", error)) {
            std::cerr << "game runtime selftest failed: " << error << '\n';
            return 1;
        }
        runtime.setInput("testMode", false);
        runtime.setInput("verifyGameOver", true);
        runtime.setInput("deltaTime", 1.0 / 60.0);
        if (!runtime.run("NativeEngine/scripts/player_system.zsc", error)) {
            std::cerr << "persistent game tick selftest failed: " << error << '\n';
            return 1;
        }
        std::cout << "Native ZS player and UI runtime self-test OK\n";
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "asset-selftest") {
        std::string error;
        if (!zumbi::AssetPackage::selfTest(error)) {
            std::cerr << "asset selftest failed: " << error << '\n';
            return 1;
        }
        std::cout << "SHA-256, Zlib, Ed25519 signing and tamper checks OK\n";
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "asset-public-key-env") {
        const std::string privateKey = environmentVariable("ZBR_ASSET_SIGNING_PRIVATE_KEY");
        if (privateKey.empty()) {
            std::cerr << "ZBR_ASSET_SIGNING_PRIVATE_KEY is not set.\n";
            return 1;
        }
        std::string publicKey, error;
        if (!zumbi::AssetPackage::derivePublicKey(privateKey, publicKey, error)) {
            std::cerr << "asset key derivation failed: " << error << '\n';
            return 1;
        }
        std::cout << publicKey << '\n';
        return 0;
    }
    if (argc == 4 && std::string(argv[1]) == "asset-pack") {
        const char* privateKey = std::getenv("ZBR_ASSET_SIGNING_PRIVATE_KEY");
        if (!privateKey || !*privateKey) {
            std::cerr << "ZBR_ASSET_SIGNING_PRIVATE_KEY is not set.\n";
            return 1;
        }
        std::string error;
        if (!zumbi::AssetPackage::pack(argv[2], argv[3], privateKey, error)) {
            std::cerr << "asset pack failed: " << error << '\n';
            return 1;
        }
        std::cout << "Signed Zlib asset package created.\n";
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "asset-verify") {
        std::string error;
        if (!zumbi::AssetPackage::verify(argv[2], ZBR_ASSET_PUBLIC_KEY_HEX, error)) {
            std::cerr << "asset verification failed: " << error << '\n';
            return 1;
        }
        std::cout << "Pinned-key asset signature and SHA-256 hashes verified.\n";
        return 0;
    }
    if (argc == 4 && std::string(argv[1]) == "asset-verify") {
        std::string error;
        if (!zumbi::AssetPackage::verify(argv[2], argv[3], error)) {
            std::cerr << "asset verification failed: " << error << '\n';
            return 1;
        }
        std::cout << "Asset signature and SHA-256 hashes verified.\n";
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "selftest") {
        std::string error;
        if (!zumbi::runEngineCoreSelfTests(error)) {
            std::cerr << "engine selftest failed: " << error << '\n';
            return 1;
        }
        std::cout << "Native world, event bus and fixed-step clock OK\\n";
        return 0;
    }
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
        std::unordered_map<std::string, zumbi::ZsValue> state;
        const zumbi::ZsNativeCall native = [&state](const std::string& name, const std::vector<zumbi::ZsValue>& args) -> zumbi::ZsValue {
            auto number = [&](std::size_t i) -> double {
                if (i >= args.size()) return -1;
                if (const auto value = std::get_if<double>(&args[i])) return *value;
                return -1;
            };
            auto string = [&](std::size_t i) -> const std::string* {
                return i < args.size() ? std::get_if<std::string>(&args[i]) : nullptr;
            };
            auto array = [&](std::size_t i) -> std::shared_ptr<zumbi::ZsArray> {
                if (i >= args.size()) return {};
                if (const auto value = std::get_if<std::shared_ptr<zumbi::ZsArray>>(&args[i])) return *value;
                return {};
            };
            auto map = [&](std::size_t i) -> std::shared_ptr<zumbi::ZsMap> {
                if (i >= args.size()) return {};
                if (const auto value = std::get_if<std::shared_ptr<zumbi::ZsMap>>(&args[i])) return *value;
                return {};
            };
            if (name == "log" || name == "print") {
                for (std::size_t i=0;i<args.size();++i) {
                    if (i) std::cout << ' ';
                    std::cout << display(args[i]);
                }
                std::cout << '\n';
                return std::monostate{};
            }
            if (name == "sqrt" && args.size() == 1) {
                const auto value = number(0);
                return value >= 0 ? zumbi::ZsValue(std::sqrt(value)) : zumbi::ZsValue(0.0);
            }
            if (name == "array.new" && args.empty()) return std::make_shared<zumbi::ZsArray>();
            if (name == "array.push" && args.size() == 2) {
                auto target = array(0);
                if (!target) return false;
                target->values.push_back(args[1]);
                return static_cast<double>(target->values.size());
            }
            if (name == "array.get" && args.size() == 2) {
                auto target = array(0);
                const auto index = number(1);
                if (!target || index < 0 || index >= static_cast<double>(target->values.size())) return std::monostate{};
                return target->values[static_cast<std::size_t>(index)];
            }
            if (name == "array.set" && args.size() == 3) {
                auto target = array(0);
                const auto index = number(1);
                if (!target || index < 0 || index >= static_cast<double>(target->values.size())) return false;
                target->values[static_cast<std::size_t>(index)] = args[2];
                return true;
            }
            if (name == "array.size" && args.size() == 1) {
                auto target = array(0);
                return target ? zumbi::ZsValue(static_cast<double>(target->values.size())) : zumbi::ZsValue(0.0);
            }
            if (name == "array.pop" && args.size() == 1) {
                auto target = array(0);
                if (!target || target->values.empty()) return std::monostate{};
                auto value = target->values.back();
                target->values.pop_back();
                return value;
            }
            if (name == "map.new" && args.empty()) return std::make_shared<zumbi::ZsMap>();
            if (name == "map.set" && args.size() == 3) {
                auto target = map(0);
                const auto key = string(1);
                if (!target || !key) return false;
                target->values[*key] = args[2];
                return args[2];
            }
            if ((name == "map.get" || name == "map.has" || name == "map.erase") && args.size() == 2) {
                auto target = map(0);
                const auto key = string(1);
                if (!target || !key) return std::monostate{};
                auto found = target->values.find(*key);
                if (name == "map.has") return found != target->values.end();
                if (found == target->values.end()) return std::monostate{};
                if (name == "map.get") return found->second;
                target->values.erase(found);
                return true;
            }
            if (name == "state.set" && args.size() == 2) {
                const auto key = string(0);
                if (!key) return false;
                state[*key] = args[1];
                return args[1];
            }
            if ((name == "state.get" || name == "state.has" || name == "state.erase") && args.size() == 1) {
                const auto key = string(0);
                if (!key) return std::monostate{};
                auto found = state.find(*key);
                if (name == "state.has") return found != state.end();
                if (found == state.end()) return std::monostate{};
                if (name == "state.get") return found->second;
                state.erase(found);
                return true;
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
