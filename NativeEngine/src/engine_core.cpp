#include "engine_core.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace zumbi {
Entity World::createEntity() {
    if (nextEntity_ == 0) return 0;
    const Entity entity = nextEntity_++;
    alive_.push_back(entity);
    transforms_.emplace(entity, Transform{});
    return entity;
}

bool World::destroyEntity(Entity entity) {
    if (!alive(entity)) return false;
    alive_.erase(std::remove(alive_.begin(), alive_.end(), entity), alive_.end());
    transforms_.erase(entity);
    return true;
}

bool World::alive(Entity entity) const {
    return entity != 0 && std::find(alive_.begin(), alive_.end(), entity) != alive_.end();
}

bool World::setTransform(Entity entity, const Transform& transform) {
    if (!alive(entity)) return false;
    transforms_[entity] = transform;
    return true;
}

const Transform* World::getTransform(Entity entity) const {
    const auto found = transforms_.find(entity);
    return found == transforms_.end() ? nullptr : &found->second;
}

std::vector<Entity> World::entities() const {
    return alive_;
}

std::size_t World::size() const {
    return alive_.size();
}

EventBus::Subscription EventBus::subscribe(const std::string& eventName, Handler handler) {
    if (eventName.empty() || !handler || nextSubscription_ == 0) return 0;
    const auto id = nextSubscription_++;
    listeners_.push_back({id, eventName, std::move(handler)});
    return id;
}

bool EventBus::unsubscribe(Subscription subscription) {
    const auto oldSize = listeners_.size();
    listeners_.erase(std::remove_if(listeners_.begin(), listeners_.end(),
        [subscription](const Listener& listener) { return listener.id == subscription; }), listeners_.end());
    return listeners_.size() != oldSize;
}

void EventBus::publish(EngineEvent event) {
    if (!event.name.empty()) queue_.push_back(std::move(event));
}

std::size_t EventBus::dispatch(std::size_t maxEvents) {
    std::size_t dispatched = 0;
    while (!queue_.empty() && dispatched < maxEvents) {
        EngineEvent event = std::move(queue_.front());
        queue_.erase(queue_.begin());
        std::vector<Handler> handlers;
        for (const auto& listener : listeners_) {
            if (listener.eventName == event.name) handlers.push_back(listener.handler);
        }
        for (const auto& handler : handlers) handler(event);
        ++dispatched;
    }
    return dispatched;
}

std::size_t EventBus::pending() const {
    return queue_.size();
}

FixedStepClock::FixedStepClock(double stepSeconds, std::uint32_t maxSteps)
    : stepSeconds_(stepSeconds > 0.0 ? stepSeconds : 1.0 / 60.0),
      maxSteps_(std::max<std::uint32_t>(1, maxSteps)) {}

std::uint32_t FixedStepClock::advance(double deltaSeconds) {
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0) return 0;
    accumulator_ += std::min(deltaSeconds, 0.25);
    std::uint32_t steps = 0;
    while (accumulator_ >= stepSeconds_ && steps < maxSteps_) {
        accumulator_ -= stepSeconds_;
        ++steps;
    }
    if (steps == maxSteps_ && accumulator_ >= stepSeconds_) accumulator_ = std::fmod(accumulator_, stepSeconds_);
    return steps;
}

double FixedStepClock::interpolation() const {
    return std::clamp(accumulator_ / stepSeconds_, 0.0, 1.0);
}

double FixedStepClock::stepSeconds() const {
    return stepSeconds_;
}

void FixedStepClock::reset() {
    accumulator_ = 0.0;
}

bool runEngineCoreSelfTests(std::string& error) {
    World world;
    const Entity first = world.createEntity();
    const Entity second = world.createEntity();
    if (!first || !second || world.size() != 2 || !world.alive(first)) {
        error = "World entity creation failed.";
        return false;
    }
    Transform transform;
    transform.position = {3.0, 4.0, 5.0};
    if (!world.setTransform(first, transform) || !world.getTransform(first) ||
        world.getTransform(first)->position.y != 4.0) {
        error = "World transform storage failed.";
        return false;
    }
    int received = 0;
    EventBus bus;
    const auto subscription = bus.subscribe("damage", [&received](const EngineEvent& event) {
        if (event.source == firstEntitySentinel && event.values.size() == 1 && event.values[0] == 12.0) ++received;
    });
    if (!subscription) {
        error = "Event subscription failed.";
        return false;
    }
    bus.publish({"damage", first, {12.0}});
    if (bus.dispatch() != 1 || received != 1 || bus.pending() != 0) {
        error = "Event dispatch failed.";
        return false;
    }
    if (!bus.unsubscribe(subscription) || bus.unsubscribe(subscription)) {
        error = "Event unsubscribe failed.";
        return false;
    }
    FixedStepClock clock;
    if (clock.advance(1.0 / 30.0) != 2 || clock.interpolation() > 1e-9) {
        error = "Fixed-step clock failed.";
        return false;
    }
    if (!world.destroyEntity(second) || world.alive(second) || world.getTransform(second)) {
        error = "World entity destruction failed.";
        return false;
    }
    return true;
}
}
