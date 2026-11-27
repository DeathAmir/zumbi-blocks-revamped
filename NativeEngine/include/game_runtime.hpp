#pragma once
#include "engine_core.hpp"
#include "zs_compiler.hpp"
#include <filesystem>
#include <random>
#include <string>
#include <unordered_map>

namespace zumbi {
struct ItemRuntimeState {
    std::uint32_t id{};
    int type{};
    int definitionId{};
    int amount{1};
    bool handgun{};
    double ammoBoxCapacity{};
    double ammo{};
    double totalAmmo{};
    std::string name;
    std::string pickupSound;
    bool alive{true};
    bool active{true};
    bool uiActive{true};
    bool colliderEnabled{true};
    bool physicsEnabled{true};
    bool equipped{};
};

struct InventoryRuntimeState {
    std::vector<std::uint32_t> slots = std::vector<std::uint32_t>(9, 0);
    int selectedSlot{1};
    int primarySlot{1};
    int secondarySlot{2};
    int meleeSlot{3};
};

struct UiNodeState {
    std::string kind;
    std::string parent;
    std::string text;
    std::string action;
    double x{};
    double y{};
    double width{};
    double height{};
    double actionArgument{};
    double value{};
    double maximum{1.0};
    double alpha{1.0};
    bool visible{true};
    bool enabled{true};
};

class NativeGameRuntime {
public:
    NativeGameRuntime();
    bool run(const std::filesystem::path& bytecode, std::string& error);
    void setInput(const std::string& name, ZsValue value);
    void setGameActive(bool active);
    const UiNodeState* uiNode(const std::string& id) const;
    std::size_t playerCount() const;
    std::size_t eventCount() const;
    bool failed() const;
    const std::string& failure() const;

private:
    ZsValue invoke(const std::string& name, const std::vector<ZsValue>& args);
    bool number(const std::vector<ZsValue>& args, std::size_t index, double& value) const;
    bool string(const std::vector<ZsValue>& args, std::size_t index, std::string& value) const;
    bool entity(const std::vector<ZsValue>& args, std::size_t index, Entity& value) const;
    void fail(const std::string& message);

    World world_;
    EventBus events_;
    std::unordered_map<Entity, std::unordered_map<std::string, ZsValue>> players_;
    std::unordered_map<Entity, InventoryRuntimeState> inventories_;
    std::unordered_map<std::uint32_t, ItemRuntimeState> items_;
    std::uint32_t nextItemId_{1};
    std::unordered_map<std::string, ZsValue> input_;
    std::unordered_map<std::string, ZsValue> state_;
    std::unordered_map<std::string, UiNodeState> ui_;
    std::mt19937 random_;
    bool gameActive_{true};
    bool failed_{};
    std::string failure_;
};
}
