#include "physics/physics_system.h"

#include "physics/physics_backend.h"

#ifdef CENGINE_ENABLE_JOLT_PHYSICS
#include "physics/jolt/jolt_physics_backend.h"
#endif

PhysicsSystem::PhysicsSystem()
{
#ifdef CENGINE_ENABLE_JOLT_PHYSICS
    backend_ = std::make_unique<JoltPhysicsBackend>();
#endif
}

PhysicsSystem::~PhysicsSystem() = default;
PhysicsSystem::PhysicsSystem(PhysicsSystem &&) noexcept = default;
PhysicsSystem &PhysicsSystem::operator=(PhysicsSystem &&) noexcept = default;

bool PhysicsSystem::Initialize(const PhysicsSystemDesc &desc)
{
    return backend_ != nullptr && backend_->Initialize(desc);
}

void PhysicsSystem::Shutdown()
{
    if (backend_ != nullptr)
    {
        backend_->Shutdown();
    }
}

void PhysicsSystem::Step(float fixed_delta_time)
{
    if (backend_ != nullptr)
    {
        backend_->Step(fixed_delta_time);
    }
}

PhysicsBodyHandle PhysicsSystem::CreateBody(const PhysicsBodyDesc &desc, const PhysicsShape &shape)
{
    return backend_ != nullptr ? backend_->CreateBody(desc, shape) : PhysicsBodyHandle{};
}

void PhysicsSystem::DestroyBody(PhysicsBodyHandle body)
{
    if (backend_ != nullptr)
    {
        backend_->DestroyBody(body);
    }
}

bool PhysicsSystem::IsValid(PhysicsBodyHandle body) const
{
    return backend_ != nullptr && backend_->IsValid(body);
}

bool PhysicsSystem::GetBodyState(PhysicsBodyHandle body, PhysicsBodyState &state) const
{
    return backend_ != nullptr && backend_->GetBodyState(body, state);
}

bool PhysicsSystem::SetBodyTransform(PhysicsBodyHandle body, const glm::vec3 &position, const glm::quat &rotation,
                                     bool activate)
{
    return backend_ != nullptr && backend_->SetBodyTransform(body, position, rotation, activate);
}

bool PhysicsSystem::MoveKinematic(PhysicsBodyHandle body, const glm::vec3 &position, const glm::quat &rotation,
                                  float fixed_delta_time)
{
    return backend_ != nullptr && backend_->MoveKinematic(body, position, rotation, fixed_delta_time);
}

bool PhysicsSystem::SetVelocity(PhysicsBodyHandle body, const glm::vec3 &linear, const glm::vec3 &angular)
{
    return backend_ != nullptr && backend_->SetVelocity(body, linear, angular);
}

bool PhysicsSystem::AddForce(PhysicsBodyHandle body, const glm::vec3 &force)
{
    return backend_ != nullptr && backend_->AddForce(body, force);
}

bool PhysicsSystem::AddForceAt(PhysicsBodyHandle body, const glm::vec3 &force, const glm::vec3 &world_position)
{
    return backend_ != nullptr && backend_->AddForceAt(body, force, world_position);
}

bool PhysicsSystem::AddTorque(PhysicsBodyHandle body, const glm::vec3 &torque)
{
    return backend_ != nullptr && backend_->AddTorque(body, torque);
}

bool PhysicsSystem::AddImpulse(PhysicsBodyHandle body, const glm::vec3 &impulse)
{
    return backend_ != nullptr && backend_->AddImpulse(body, impulse);
}

bool PhysicsSystem::AddImpulseAt(PhysicsBodyHandle body, const glm::vec3 &impulse, const glm::vec3 &world_position)
{
    return backend_ != nullptr && backend_->AddImpulseAt(body, impulse, world_position);
}

bool PhysicsSystem::SetGravityFactor(PhysicsBodyHandle body, float factor)
{
    return backend_ != nullptr && backend_->SetGravityFactor(body, factor);
}

bool PhysicsSystem::SetSensor(PhysicsBodyHandle body, bool sensor)
{
    return backend_ != nullptr && backend_->SetSensor(body, sensor);
}

bool PhysicsSystem::Activate(PhysicsBodyHandle body)
{
    return backend_ != nullptr && backend_->Activate(body);
}

bool PhysicsSystem::SetLayerCollision(std::uint8_t first, std::uint8_t second, bool enabled)
{
    return backend_ != nullptr && backend_->SetLayerCollision(first, second, enabled);
}

bool PhysicsSystem::RayCast(const glm::vec3 &origin, const glm::vec3 &displacement, PhysicsQueryHit &hit,
                            std::uint32_t layer_mask, PhysicsBodyHandle ignore) const
{
    return backend_ != nullptr && backend_->RayCast(origin, displacement, hit, layer_mask, ignore);
}

std::size_t PhysicsSystem::RayCastAll(const glm::vec3 &origin, const glm::vec3 &displacement, PhysicsQueryHit *hits,
                                      std::size_t capacity, std::uint32_t layer_mask, PhysicsBodyHandle ignore) const
{
    return backend_ != nullptr ? backend_->RayCastAll(origin, displacement, hits, capacity, layer_mask, ignore) : 0;
}

bool PhysicsSystem::ShapeCast(const PhysicsShape &shape, const glm::vec3 &position, const glm::quat &rotation,
                              const glm::vec3 &displacement, PhysicsQueryHit &hit, std::uint32_t layer_mask,
                              PhysicsBodyHandle ignore) const
{
    return backend_ != nullptr && backend_->ShapeCast(shape, position, rotation, displacement, hit, layer_mask, ignore);
}

std::size_t PhysicsSystem::Overlap(const PhysicsShape &shape, const glm::vec3 &position, const glm::quat &rotation,
                                   PhysicsBodyHandle *bodies, std::size_t capacity, std::uint32_t layer_mask,
                                   PhysicsBodyHandle ignore) const
{
    return backend_ != nullptr ? backend_->Overlap(shape, position, rotation, bodies, capacity, layer_mask, ignore) : 0;
}

std::size_t PhysicsSystem::DrainContactEvents(PhysicsContactEvent *events, std::size_t capacity)
{
    return backend_ != nullptr ? backend_->DrainContactEvents(events, capacity) : 0;
}

std::size_t PhysicsSystem::PendingContactEventCount() const
{
    return backend_ != nullptr ? backend_->PendingContactEventCount() : 0;
}

std::uint64_t PhysicsSystem::DroppedContactEventCount() const
{
    return backend_ != nullptr ? backend_->DroppedContactEventCount() : 0;
}

PhysicsConstraintHandle PhysicsSystem::CreateConstraint(const PhysicsConstraintDesc &desc)
{
    return backend_ != nullptr ? backend_->CreateConstraint(desc) : PhysicsConstraintHandle{};
}

void PhysicsSystem::DestroyConstraint(PhysicsConstraintHandle constraint)
{
    if (backend_ != nullptr)
    {
        backend_->DestroyConstraint(constraint);
    }
}

bool PhysicsSystem::SetConstraintEnabled(PhysicsConstraintHandle constraint, bool enabled)
{
    return backend_ != nullptr && backend_->SetConstraintEnabled(constraint, enabled);
}

bool PhysicsSystem::SetConstraintMotor(PhysicsConstraintHandle constraint, PhysicsMotorMode mode, float target,
                                       float force_limit, float frequency, float damping)
{
    return backend_ != nullptr &&
           backend_->SetConstraintMotor(constraint, mode, target, force_limit, frequency, damping);
}

bool PhysicsSystem::IsValid(PhysicsConstraintHandle constraint) const
{
    return backend_ != nullptr && backend_->IsValid(constraint);
}

std::size_t PhysicsSystem::ConstraintCount() const
{
    return backend_ != nullptr ? backend_->ConstraintCount() : 0;
}

PhysicsCharacterHandle PhysicsSystem::CreateCharacter(const PhysicsCharacterDesc &desc)
{
    return backend_ != nullptr ? backend_->CreateCharacter(desc) : PhysicsCharacterHandle{};
}

void PhysicsSystem::DestroyCharacter(PhysicsCharacterHandle character)
{
    if (backend_ != nullptr)
    {
        backend_->DestroyCharacter(character);
    }
}

bool PhysicsSystem::IsValid(PhysicsCharacterHandle character) const
{
    return backend_ != nullptr && backend_->IsValid(character);
}

bool PhysicsSystem::SetCharacterVelocity(PhysicsCharacterHandle character, const glm::vec3 &velocity)
{
    return backend_ != nullptr && backend_->SetCharacterVelocity(character, velocity);
}

bool PhysicsSystem::UpdateCharacter(PhysicsCharacterHandle character, float fixed_delta_time)
{
    return backend_ != nullptr && backend_->UpdateCharacter(character, fixed_delta_time);
}

bool PhysicsSystem::SetCharacterHeight(PhysicsCharacterHandle character, float height)
{
    return backend_ != nullptr && backend_->SetCharacterHeight(character, height);
}

bool PhysicsSystem::GetCharacterState(PhysicsCharacterHandle character, PhysicsCharacterState &state) const
{
    return backend_ != nullptr && backend_->GetCharacterState(character, state);
}

std::size_t PhysicsSystem::CharacterCount() const
{
    return backend_ != nullptr ? backend_->CharacterCount() : 0;
}

std::size_t PhysicsSystem::BodyCount() const
{
    return backend_ != nullptr ? backend_->BodyCount() : 0;
}
