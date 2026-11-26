#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace zumbi {
using Entity = std::uint32_t;

struct Vec3 {
    double x{};
    double y{};
    double z{};
};

struct Transform {
    Vec3 position{};
    Vec3 rotation{};
    Vec3 scale{1.0, 1.0, 1.0};
};

struct EngineEvent {
    std::string name;
    Entity source{};
    std::vector<double> values;
};

class World {
public:
    Entity createEntity();
    bool destroyEntity(Entity entity);
    bool alive(Entity entity) const;
    bool setTransform(Entity entity, const Transform& transform);
    const Transform* getTransform(Entity entity) const;
    std::vector<Entity> entities() const;
    std::size_t size() const;

private:
    Entity nextEntity_{1};
    std::vector<Entity> alive_;
    std::unordered_map<Entity, Transform> transforms_;
};

class EventBus {
public:
    using Handler = std::function<void(const EngineEvent&)>;
    using Subscription = std::uint64_t;

    Subscription subscribe(const std::string& eventName, Handler handler);
    bool unsubscribe(Subscription subscription);
    void publish(EngineEvent event);
    std::size_t dispatch(std::size_t maxEvents = 4096);
    std::size_t pending() const;

private:
    struct Listener {
        Subscription id{};
        std::string eventName;
        Handler handler;
    };
    Subscription nextSubscription_{1};
    std::vector<Listener> listeners_;
    std::vector<EngineEvent> queue_;
};

class FixedStepClock {
public:
    explicit FixedStepClock(double stepSeconds = 1.0 / 60.0, std::uint32_t maxSteps = 8);
    std::uint32_t advance(double deltaSeconds);
    double interpolation() const;
    double stepSeconds() const;
    void reset();

private:
    double stepSeconds_;
    double accumulator_{};
    std::uint32_t maxSteps_;
};

bool runEngineCoreSelfTests(std::string& error);
}
