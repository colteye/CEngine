#ifndef CENGINE_SCENE_SCENE_PHYSICS_STATE_H
#define CENGINE_SCENE_SCENE_PHYSICS_STATE_H

#include "entity/entity.h"
#include "physics/physics_types.h"

#include <string>
#include <vector>

class PhysicsSystem;

namespace CEngine::Scene {

class Scene;

class ScenePhysicsState {
public:
    ~ScenePhysicsState();

    bool Activate(Scene& scene, PhysicsSystem& physics, std::string* error = nullptr);
    void Step(float delta_seconds);
    void Stop();
    bool Active() const { return physics_ != nullptr; }

private:
    struct DynamicBody {
        EntityId entity;
        PhysicsBodyHandle body = kInvalidPhysicsBodyHandle;
    };

    Scene* scene_ = nullptr;
    PhysicsSystem* physics_ = nullptr;
    std::vector<DynamicBody> dynamic_bodies_;
};

} // namespace CEngine::Scene

#endif
