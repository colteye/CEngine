#include "scene/scene_physics_state.h"

#include "entity/dynamic_prop.h"
#include "physics/physics_system.h"
#include "scene/scene.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace CEngine::Scene {

ScenePhysicsState::~ScenePhysicsState()
{
    Stop();
}

bool ScenePhysicsState::Activate(Scene& scene, PhysicsSystem& physics, std::string* error)
{
    Stop();
    scene_ = &scene;
    physics_ = &physics;
    dynamic_bodies_.reserve(scene.EntityCount());
    for (const auto& entity : scene.Entities())
    {
        if (entity == nullptr || entity->Classname() != "prop_dynamic") continue;
        const auto& prop = static_cast<const Entities::DynamicProp&>(*entity);
        if (!prop.physics_enabled || !prop.Enabled()) continue;
        PhysicsBodyDesc desc;
        desc.motion_type = PhysicsMotionType::Dynamic;
        desc.shape_type = PhysicsShapeType::Box;
        desc.position = prop.GetTransform().position;
        desc.rotation = prop.GetTransform().rotation;
        desc.box_half_extents = glm::abs(prop.GetTransform().scale) * prop.collision_half_extents;
        desc.mass = prop.mass;
        const PhysicsBodyHandle body = physics.CreateBody(desc);
        if (body == kInvalidPhysicsBodyHandle)
        {
            if (error != nullptr) *error = "failed to create physics body for dynamic prop: " + prop.Name();
            Stop();
            return false;
        }
        dynamic_bodies_.push_back({prop.Id(), body});
    }
    return true;
}

void ScenePhysicsState::Step(float delta_seconds)
{
    if (scene_ == nullptr || physics_ == nullptr) return;
    physics_->Step(delta_seconds);
    for (const DynamicBody& dynamic : dynamic_bodies_)
    {
        Entity* entity = scene_->GetEntity(dynamic.entity);
        if (entity == nullptr) continue;
        const glm::mat4 world = physics_->GetBodyTransform(dynamic.body);
        entity->GetTransform().position = glm::vec3(world[3]);
        entity->GetTransform().rotation = glm::quat_cast(world);
        entity->GetTransform().world_matrix = world * glm::scale(glm::mat4(1.0f), entity->GetTransform().scale);
        entity->GetTransform().dirty = false;
    }
}

void ScenePhysicsState::Stop()
{
    if (physics_ != nullptr)
        for (auto body = dynamic_bodies_.rbegin(); body != dynamic_bodies_.rend(); ++body)
            physics_->DestroyBody(body->body);
    dynamic_bodies_.clear();
    scene_ = nullptr;
    physics_ = nullptr;
}

} // namespace CEngine::Scene
