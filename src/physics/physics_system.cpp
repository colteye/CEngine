//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/physics/physics_system.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "physics/physics_system.h"

#include "physics/physics_backend.h"

#ifdef CENGINE_ENABLE_JOLT_PHYSICS
#include "physics/jolt/jolt_physics_backend.h"
#endif

/**
 * @brief TODO: Describe PhysicsSystem::PhysicsSystem.
 */
PhysicsSystem::PhysicsSystem()
{
#ifdef CENGINE_ENABLE_JOLT_PHYSICS
    backend_ = std::make_unique<JoltPhysicsBackend>();
#endif
}

/**
 * @brief TODO: Describe PhysicsSystem::~PhysicsSystem.
 */
PhysicsSystem::~PhysicsSystem() = default;
/**
 * @brief TODO: Describe PhysicsSystem::PhysicsSystem.
 */
PhysicsSystem::PhysicsSystem(PhysicsSystem &&) noexcept = default;
/**
 * @brief TODO: Describe operator=.
 *
 * @return TODO: Describe the return value.
 */
PhysicsSystem &PhysicsSystem::operator=(PhysicsSystem &&) noexcept = default;

/**
 * @brief TODO: Describe PhysicsSystem::Initialize.
 *
 * @param desc TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool PhysicsSystem::Initialize(const PhysicsSystemDesc &desc)
{
    return backend_ != nullptr && backend_->Initialize(desc);
}

/**
 * @brief TODO: Describe PhysicsSystem::Shutdown.
 */
void PhysicsSystem::Shutdown()
{
    if (backend_ != nullptr)
    {
        backend_->Shutdown();
    }
}

/**
 * @brief TODO: Describe PhysicsSystem::Step.
 *
 * @param fixed_delta_time TODO: Describe this parameter.
 */
void PhysicsSystem::Step(float fixed_delta_time)
{
    if (backend_ != nullptr)
    {
        backend_->Step(fixed_delta_time);
    }
}

/**
 * @brief TODO: Describe PhysicsSystem::CreateBody.
 *
 * @param desc TODO: Describe this parameter.
 * @param shape TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
PhysicsBodyHandle PhysicsSystem::CreateBody(const PhysicsBodyDesc &desc, const PhysicsShape &shape)
{
    return backend_ != nullptr ? backend_->CreateBody(desc, shape) : PhysicsBodyHandle{};
}

/**
 * @brief TODO: Describe PhysicsSystem::DestroyBody.
 *
 * @param body TODO: Describe this parameter.
 */
void PhysicsSystem::DestroyBody(PhysicsBodyHandle body)
{
    if (backend_ != nullptr)
    {
        backend_->DestroyBody(body);
    }
}

/**
 * @brief TODO: Describe PhysicsSystem::IsValid.
 *
 * @param body TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool PhysicsSystem::IsValid(PhysicsBodyHandle body) const
{
    return backend_ != nullptr && backend_->IsValid(body);
}

/**
 * @brief TODO: Describe PhysicsSystem::GetBodyState.
 *
 * @param body TODO: Describe this parameter.
 * @param state TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool PhysicsSystem::GetBodyState(PhysicsBodyHandle body, PhysicsBodyState &state) const
{
    return backend_ != nullptr && backend_->GetBodyState(body, state);
}

/**
 * @brief TODO: Describe PhysicsSystem::SetBodyTransform.
 *
 * @param body TODO: Describe this parameter.
 * @param position TODO: Describe this parameter.
 * @param rotation TODO: Describe this parameter.
 * @param activate TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool PhysicsSystem::SetBodyTransform(PhysicsBodyHandle body, const glm::vec3 &position, const glm::quat &rotation,
                                     bool activate)
{
    return backend_ != nullptr && backend_->SetBodyTransform(body, position, rotation, activate);
}

/**
 * @brief TODO: Describe PhysicsSystem::MoveKinematic.
 *
 * @param body TODO: Describe this parameter.
 * @param position TODO: Describe this parameter.
 * @param rotation TODO: Describe this parameter.
 * @param fixed_delta_time TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool PhysicsSystem::MoveKinematic(PhysicsBodyHandle body, const glm::vec3 &position, const glm::quat &rotation,
                                  float fixed_delta_time)
{
    return backend_ != nullptr && backend_->MoveKinematic(body, position, rotation, fixed_delta_time);
}

/**
 * @brief TODO: Describe PhysicsSystem::SetVelocity.
 *
 * @param body TODO: Describe this parameter.
 * @param linear TODO: Describe this parameter.
 * @param angular TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool PhysicsSystem::SetVelocity(PhysicsBodyHandle body, const glm::vec3 &linear, const glm::vec3 &angular)
{
    return backend_ != nullptr && backend_->SetVelocity(body, linear, angular);
}

/**
 * @brief TODO: Describe PhysicsSystem::AddForce.
 *
 * @param body TODO: Describe this parameter.
 * @param force TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool PhysicsSystem::AddForce(PhysicsBodyHandle body, const glm::vec3 &force)
{
    return backend_ != nullptr && backend_->AddForce(body, force);
}

/**
 * @brief TODO: Describe PhysicsSystem::AddForceAt.
 *
 * @param body TODO: Describe this parameter.
 * @param force TODO: Describe this parameter.
 * @param world_position TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool PhysicsSystem::AddForceAt(PhysicsBodyHandle body, const glm::vec3 &force, const glm::vec3 &world_position)
{
    return backend_ != nullptr && backend_->AddForceAt(body, force, world_position);
}

/**
 * @brief TODO: Describe PhysicsSystem::AddTorque.
 *
 * @param body TODO: Describe this parameter.
 * @param torque TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool PhysicsSystem::AddTorque(PhysicsBodyHandle body, const glm::vec3 &torque)
{
    return backend_ != nullptr && backend_->AddTorque(body, torque);
}

/**
 * @brief TODO: Describe PhysicsSystem::AddImpulse.
 *
 * @param body TODO: Describe this parameter.
 * @param impulse TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool PhysicsSystem::AddImpulse(PhysicsBodyHandle body, const glm::vec3 &impulse)
{
    return backend_ != nullptr && backend_->AddImpulse(body, impulse);
}

/**
 * @brief TODO: Describe PhysicsSystem::AddImpulseAt.
 *
 * @param body TODO: Describe this parameter.
 * @param impulse TODO: Describe this parameter.
 * @param world_position TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool PhysicsSystem::AddImpulseAt(PhysicsBodyHandle body, const glm::vec3 &impulse, const glm::vec3 &world_position)
{
    return backend_ != nullptr && backend_->AddImpulseAt(body, impulse, world_position);
}

/**
 * @brief TODO: Describe PhysicsSystem::SetGravityFactor.
 *
 * @param body TODO: Describe this parameter.
 * @param factor TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool PhysicsSystem::SetGravityFactor(PhysicsBodyHandle body, float factor)
{
    return backend_ != nullptr && backend_->SetGravityFactor(body, factor);
}

/**
 * @brief TODO: Describe PhysicsSystem::SetSensor.
 *
 * @param body TODO: Describe this parameter.
 * @param sensor TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool PhysicsSystem::SetSensor(PhysicsBodyHandle body, bool sensor)
{
    return backend_ != nullptr && backend_->SetSensor(body, sensor);
}

/**
 * @brief TODO: Describe PhysicsSystem::Activate.
 *
 * @param body TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool PhysicsSystem::Activate(PhysicsBodyHandle body)
{
    return backend_ != nullptr && backend_->Activate(body);
}

/**
 * @brief TODO: Describe PhysicsSystem::SetLayerCollision.
 *
 * @param first TODO: Describe this parameter.
 * @param second TODO: Describe this parameter.
 * @param enabled TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool PhysicsSystem::SetLayerCollision(std::uint8_t first, std::uint8_t second, bool enabled)
{
    return backend_ != nullptr && backend_->SetLayerCollision(first, second, enabled);
}

/**
 * @brief TODO: Describe PhysicsSystem::RayCast.
 *
 * @param origin TODO: Describe this parameter.
 * @param displacement TODO: Describe this parameter.
 * @param hit TODO: Describe this parameter.
 * @param layer_mask TODO: Describe this parameter.
 * @param ignore TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool PhysicsSystem::RayCast(const glm::vec3 &origin, const glm::vec3 &displacement, PhysicsQueryHit &hit,
                            std::uint32_t layer_mask, PhysicsBodyHandle ignore) const
{
    return backend_ != nullptr && backend_->RayCast(origin, displacement, hit, layer_mask, ignore);
}

/**
 * @brief TODO: Describe PhysicsSystem::RayCastAll.
 *
 * @param origin TODO: Describe this parameter.
 * @param displacement TODO: Describe this parameter.
 * @param hits TODO: Describe this parameter.
 * @param capacity TODO: Describe this parameter.
 * @param layer_mask TODO: Describe this parameter.
 * @param ignore TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
std::size_t PhysicsSystem::RayCastAll(const glm::vec3 &origin, const glm::vec3 &displacement, PhysicsQueryHit *hits,
                                      std::size_t capacity, std::uint32_t layer_mask, PhysicsBodyHandle ignore) const
{
    return backend_ != nullptr ? backend_->RayCastAll(origin, displacement, hits, capacity, layer_mask, ignore) : 0;
}

/**
 * @brief TODO: Describe PhysicsSystem::ShapeCast.
 *
 * @param shape TODO: Describe this parameter.
 * @param position TODO: Describe this parameter.
 * @param rotation TODO: Describe this parameter.
 * @param displacement TODO: Describe this parameter.
 * @param hit TODO: Describe this parameter.
 * @param layer_mask TODO: Describe this parameter.
 * @param ignore TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool PhysicsSystem::ShapeCast(const PhysicsShape &shape, const glm::vec3 &position, const glm::quat &rotation,
                              const glm::vec3 &displacement, PhysicsQueryHit &hit, std::uint32_t layer_mask,
                              PhysicsBodyHandle ignore) const
{
    return backend_ != nullptr && backend_->ShapeCast(shape, position, rotation, displacement, hit, layer_mask, ignore);
}

/**
 * @brief TODO: Describe PhysicsSystem::Overlap.
 *
 * @param shape TODO: Describe this parameter.
 * @param position TODO: Describe this parameter.
 * @param rotation TODO: Describe this parameter.
 * @param bodies TODO: Describe this parameter.
 * @param capacity TODO: Describe this parameter.
 * @param layer_mask TODO: Describe this parameter.
 * @param ignore TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
std::size_t PhysicsSystem::Overlap(const PhysicsShape &shape, const glm::vec3 &position, const glm::quat &rotation,
                                   PhysicsBodyHandle *bodies, std::size_t capacity, std::uint32_t layer_mask,
                                   PhysicsBodyHandle ignore) const
{
    return backend_ != nullptr ? backend_->Overlap(shape, position, rotation, bodies, capacity, layer_mask, ignore) : 0;
}

/**
 * @brief TODO: Describe PhysicsSystem::DrainContactEvents.
 *
 * @param events TODO: Describe this parameter.
 * @param capacity TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
std::size_t PhysicsSystem::DrainContactEvents(PhysicsContactEvent *events, std::size_t capacity)
{
    return backend_ != nullptr ? backend_->DrainContactEvents(events, capacity) : 0;
}

/**
 * @brief TODO: Describe PhysicsSystem::PendingContactEventCount.
 *
 * @return TODO: Describe the return value.
 */
std::size_t PhysicsSystem::PendingContactEventCount() const
{
    return backend_ != nullptr ? backend_->PendingContactEventCount() : 0;
}

/**
 * @brief TODO: Describe PhysicsSystem::DroppedContactEventCount.
 *
 * @return TODO: Describe the return value.
 */
std::uint64_t PhysicsSystem::DroppedContactEventCount() const
{
    return backend_ != nullptr ? backend_->DroppedContactEventCount() : 0;
}

/**
 * @brief TODO: Describe PhysicsSystem::CreateConstraint.
 *
 * @param desc TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
PhysicsConstraintHandle PhysicsSystem::CreateConstraint(const PhysicsConstraintDesc &desc)
{
    return backend_ != nullptr ? backend_->CreateConstraint(desc) : PhysicsConstraintHandle{};
}

/**
 * @brief TODO: Describe PhysicsSystem::DestroyConstraint.
 *
 * @param constraint TODO: Describe this parameter.
 */
void PhysicsSystem::DestroyConstraint(PhysicsConstraintHandle constraint)
{
    if (backend_ != nullptr)
    {
        backend_->DestroyConstraint(constraint);
    }
}

/**
 * @brief TODO: Describe PhysicsSystem::SetConstraintEnabled.
 *
 * @param constraint TODO: Describe this parameter.
 * @param enabled TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool PhysicsSystem::SetConstraintEnabled(PhysicsConstraintHandle constraint, bool enabled)
{
    return backend_ != nullptr && backend_->SetConstraintEnabled(constraint, enabled);
}

/**
 * @brief TODO: Describe PhysicsSystem::SetConstraintMotor.
 *
 * @param constraint TODO: Describe this parameter.
 * @param mode TODO: Describe this parameter.
 * @param target TODO: Describe this parameter.
 * @param force_limit TODO: Describe this parameter.
 * @param frequency TODO: Describe this parameter.
 * @param damping TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool PhysicsSystem::SetConstraintMotor(PhysicsConstraintHandle constraint, PhysicsMotorMode mode, float target,
                                       float force_limit, float frequency, float damping)
{
    return backend_ != nullptr &&
           backend_->SetConstraintMotor(constraint, mode, target, force_limit, frequency, damping);
}

/**
 * @brief TODO: Describe PhysicsSystem::IsValid.
 *
 * @param constraint TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool PhysicsSystem::IsValid(PhysicsConstraintHandle constraint) const
{
    return backend_ != nullptr && backend_->IsValid(constraint);
}

/**
 * @brief TODO: Describe PhysicsSystem::ConstraintCount.
 *
 * @return TODO: Describe the return value.
 */
std::size_t PhysicsSystem::ConstraintCount() const
{
    return backend_ != nullptr ? backend_->ConstraintCount() : 0;
}

/**
 * @brief TODO: Describe PhysicsSystem::CreateCharacter.
 *
 * @param desc TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
PhysicsCharacterHandle PhysicsSystem::CreateCharacter(const PhysicsCharacterDesc &desc)
{
    return backend_ != nullptr ? backend_->CreateCharacter(desc) : PhysicsCharacterHandle{};
}

/**
 * @brief TODO: Describe PhysicsSystem::DestroyCharacter.
 *
 * @param character TODO: Describe this parameter.
 */
void PhysicsSystem::DestroyCharacter(PhysicsCharacterHandle character)
{
    if (backend_ != nullptr)
    {
        backend_->DestroyCharacter(character);
    }
}

/**
 * @brief TODO: Describe PhysicsSystem::IsValid.
 *
 * @param character TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool PhysicsSystem::IsValid(PhysicsCharacterHandle character) const
{
    return backend_ != nullptr && backend_->IsValid(character);
}

/**
 * @brief TODO: Describe PhysicsSystem::SetCharacterVelocity.
 *
 * @param character TODO: Describe this parameter.
 * @param velocity TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool PhysicsSystem::SetCharacterVelocity(PhysicsCharacterHandle character, const glm::vec3 &velocity)
{
    return backend_ != nullptr && backend_->SetCharacterVelocity(character, velocity);
}

/**
 * @brief TODO: Describe PhysicsSystem::UpdateCharacter.
 *
 * @param character TODO: Describe this parameter.
 * @param fixed_delta_time TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool PhysicsSystem::UpdateCharacter(PhysicsCharacterHandle character, float fixed_delta_time)
{
    return backend_ != nullptr && backend_->UpdateCharacter(character, fixed_delta_time);
}

/**
 * @brief TODO: Describe PhysicsSystem::SetCharacterHeight.
 *
 * @param character TODO: Describe this parameter.
 * @param height TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool PhysicsSystem::SetCharacterHeight(PhysicsCharacterHandle character, float height)
{
    return backend_ != nullptr && backend_->SetCharacterHeight(character, height);
}

/**
 * @brief TODO: Describe PhysicsSystem::GetCharacterState.
 *
 * @param character TODO: Describe this parameter.
 * @param state TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool PhysicsSystem::GetCharacterState(PhysicsCharacterHandle character, PhysicsCharacterState &state) const
{
    return backend_ != nullptr && backend_->GetCharacterState(character, state);
}

/**
 * @brief TODO: Describe PhysicsSystem::CharacterCount.
 *
 * @return TODO: Describe the return value.
 */
std::size_t PhysicsSystem::CharacterCount() const
{
    return backend_ != nullptr ? backend_->CharacterCount() : 0;
}

/**
 * @brief TODO: Describe PhysicsSystem::BodyCount.
 *
 * @return TODO: Describe the return value.
 */
std::size_t PhysicsSystem::BodyCount() const
{
    return backend_ != nullptr ? backend_->BodyCount() : 0;
}
