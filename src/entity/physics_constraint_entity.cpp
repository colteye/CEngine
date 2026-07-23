#include "entity/physics_constraint_entity.h"

#include "context.h"
#include "entity/prop_entity.h"
#include "physics/physics_system.h"
#include "scene/scene.h"

#include <stdexcept>

namespace CEngine::Entities
{
namespace
{

PhysicsConstraintType ConstraintType(Generated::EngineEntities::PhysicsConstraintKind value)
{
    switch (value)
    {
    case Generated::EngineEntities::PhysicsConstraintKind::Fixed:
        return PhysicsConstraintType::Fixed;
    case Generated::EngineEntities::PhysicsConstraintKind::Point:
        return PhysicsConstraintType::Point;
    case Generated::EngineEntities::PhysicsConstraintKind::Hinge:
        return PhysicsConstraintType::Hinge;
    case Generated::EngineEntities::PhysicsConstraintKind::Slider:
        return PhysicsConstraintType::Slider;
    case Generated::EngineEntities::PhysicsConstraintKind::Distance:
        return PhysicsConstraintType::Distance;
    case Generated::EngineEntities::PhysicsConstraintKind::Cone:
        return PhysicsConstraintType::Cone;
    }
    throw std::runtime_error("physics constraint type is unsupported");
}

PhysicsMotorMode MotorMode(Generated::EngineEntities::PhysicsMotorKind value)
{
    switch (value)
    {
    case Generated::EngineEntities::PhysicsMotorKind::Off:
        return PhysicsMotorMode::Off;
    case Generated::EngineEntities::PhysicsMotorKind::Velocity:
        return PhysicsMotorMode::Velocity;
    case Generated::EngineEntities::PhysicsMotorKind::Position:
        return PhysicsMotorMode::Position;
    }
    throw std::runtime_error("physics motor type is unsupported");
}

PropEntity *BodyEntity(Scene::Scene &scene, std::uint32_t index)
{
    if (index >= scene.Entities().size())
    {
        return nullptr;
    }
    return dynamic_cast<PropEntity *>(scene.Entities()[index].get());
}

} // namespace

std::string_view PhysicsConstraintEntity::Classname() const
{
    return "physics_constraint";
}

void PhysicsConstraintEntity::Initialize(Context &context)
{
    if (context.physics == nullptr || context.scene == nullptr)
    {
        throw std::runtime_error("physics constraint requires an active scene and physics system");
    }

    PropEntity *first = BodyEntity(*context.scene, first_entity);
    PropEntity *second = BodyEntity(*context.scene, second_entity);
    if (first == nullptr || second == nullptr || !first->PhysicsBody() || !second->PhysicsBody())
    {
        throw std::runtime_error("physics constraint must reference two initialized physics props");
    }

    PhysicsConstraintDesc desc;
    desc.type = ConstraintType(type);
    desc.first = first->PhysicsBody();
    desc.second = second->PhysicsBody();
    desc.first_anchor = GetTransform().position;
    desc.second_anchor = {second_anchor.x, second_anchor.y, second_anchor.z};
    desc.axis = {axis.x, axis.y, axis.z};
    desc.normal = {normal.x, normal.y, normal.z};
    desc.minimum = minimum;
    desc.maximum = maximum;
    desc.spring_frequency = spring_frequency;
    desc.spring_damping = spring_damping;
    desc.motor = MotorMode(motor);
    desc.motor_target = motor_target;
    desc.motor_force_limit = motor_force_limit;
    desc.motor_frequency = motor_frequency;
    desc.motor_damping = motor_damping;
    desc.enabled = enabled;
    constraint_ = context.physics->CreateConstraint(desc);
    if (!constraint_)
    {
        throw std::runtime_error("physics constraint configuration is invalid");
    }
}

void PhysicsConstraintEntity::Shutdown(Context &context)
{
    if (context.physics != nullptr && constraint_)
    {
        context.physics->DestroyConstraint(constraint_);
    }
    constraint_ = {};
}

} // namespace CEngine::Entities
