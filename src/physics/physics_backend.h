//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/physics/physics_backend.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_PHYSICS_PHYSICS_BACKEND_H
#define CENGINE_PHYSICS_PHYSICS_BACKEND_H

#include "physics/physics_types.h"

#include <cstddef>
#include <cstdint>

/**
 * @brief TODO: Describe IPhysicsBackend.
 */
class IPhysicsBackend
{
  public:
    /**
     * @brief TODO: Describe IPhysicsBackend.
     */
    IPhysicsBackend() = default;
    /**
     * @brief TODO: Describe ~IPhysicsBackend.
     */
    virtual ~IPhysicsBackend() = default;
    /**
     * @brief TODO: Describe IPhysicsBackend.
     */
    IPhysicsBackend(const IPhysicsBackend &) = delete;
    /**
     * @brief TODO: Describe operator=.
     *
     * @return TODO: Describe the return value.
     */
    IPhysicsBackend &operator=(const IPhysicsBackend &) = delete;
    /**
     * @brief TODO: Describe IPhysicsBackend.
     */
    IPhysicsBackend(IPhysicsBackend &&) = default;
    /**
     * @brief TODO: Describe operator=.
     *
     * @return TODO: Describe the return value.
     */
    IPhysicsBackend &operator=(IPhysicsBackend &&) = default;

    /**
     * @brief TODO: Describe Initialize.
     *
     * @param desc TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    virtual bool Initialize(const PhysicsSystemDesc &desc) = 0;
    /**
     * @brief TODO: Describe Shutdown.
     */
    virtual void Shutdown() = 0;
    /**
     * @brief TODO: Describe Step.
     *
     * @param fixed_delta_time TODO: Describe this parameter.
     */
    virtual void Step(float fixed_delta_time) = 0;

    /**
     * @brief TODO: Describe CreateBody.
     *
     * @param desc TODO: Describe this parameter.
     * @param shape TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    virtual PhysicsBodyHandle CreateBody(const PhysicsBodyDesc &desc, const PhysicsShape &shape) = 0;
    /**
     * @brief TODO: Describe DestroyBody.
     *
     * @param body TODO: Describe this parameter.
     */
    virtual void DestroyBody(PhysicsBodyHandle body) = 0;
    /**
     * @brief TODO: Describe IsValid.
     *
     * @param body TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] virtual bool IsValid(PhysicsBodyHandle body) const = 0;
    /**
     * @brief TODO: Describe GetBodyState.
     *
     * @param body TODO: Describe this parameter.
     * @param state TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    virtual bool GetBodyState(PhysicsBodyHandle body, PhysicsBodyState &state) const = 0;
    /**
     * @brief TODO: Describe SetBodyTransform.
     *
     * @param body TODO: Describe this parameter.
     * @param position TODO: Describe this parameter.
     * @param rotation TODO: Describe this parameter.
     * @param activate TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    virtual bool SetBodyTransform(PhysicsBodyHandle body, const glm::vec3 &position, const glm::quat &rotation,
                                  bool activate) = 0;
    /**
     * @brief TODO: Describe MoveKinematic.
     *
     * @param body TODO: Describe this parameter.
     * @param position TODO: Describe this parameter.
     * @param rotation TODO: Describe this parameter.
     * @param fixed_delta_time TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    virtual bool MoveKinematic(PhysicsBodyHandle body, const glm::vec3 &position, const glm::quat &rotation,
                               float fixed_delta_time) = 0;
    /**
     * @brief TODO: Describe SetVelocity.
     *
     * @param body TODO: Describe this parameter.
     * @param linear TODO: Describe this parameter.
     * @param angular TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    virtual bool SetVelocity(PhysicsBodyHandle body, const glm::vec3 &linear, const glm::vec3 &angular) = 0;
    /**
     * @brief TODO: Describe AddForce.
     *
     * @param body TODO: Describe this parameter.
     * @param force TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    virtual bool AddForce(PhysicsBodyHandle body, const glm::vec3 &force) = 0;
    /**
     * @brief TODO: Describe AddForceAt.
     *
     * @param body TODO: Describe this parameter.
     * @param force TODO: Describe this parameter.
     * @param world_position TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    virtual bool AddForceAt(PhysicsBodyHandle body, const glm::vec3 &force, const glm::vec3 &world_position) = 0;
    /**
     * @brief TODO: Describe AddTorque.
     *
     * @param body TODO: Describe this parameter.
     * @param torque TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    virtual bool AddTorque(PhysicsBodyHandle body, const glm::vec3 &torque) = 0;
    /**
     * @brief TODO: Describe AddImpulse.
     *
     * @param body TODO: Describe this parameter.
     * @param impulse TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    virtual bool AddImpulse(PhysicsBodyHandle body, const glm::vec3 &impulse) = 0;
    /**
     * @brief TODO: Describe AddImpulseAt.
     *
     * @param body TODO: Describe this parameter.
     * @param impulse TODO: Describe this parameter.
     * @param world_position TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    virtual bool AddImpulseAt(PhysicsBodyHandle body, const glm::vec3 &impulse, const glm::vec3 &world_position) = 0;
    /**
     * @brief TODO: Describe SetGravityFactor.
     *
     * @param body TODO: Describe this parameter.
     * @param factor TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    virtual bool SetGravityFactor(PhysicsBodyHandle body, float factor) = 0;
    /**
     * @brief TODO: Describe SetSensor.
     *
     * @param body TODO: Describe this parameter.
     * @param sensor TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    virtual bool SetSensor(PhysicsBodyHandle body, bool sensor) = 0;
    /**
     * @brief TODO: Describe Activate.
     *
     * @param body TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    virtual bool Activate(PhysicsBodyHandle body) = 0;
    /**
     * @brief TODO: Describe SetLayerCollision.
     *
     * @param first TODO: Describe this parameter.
     * @param second TODO: Describe this parameter.
     * @param enabled TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    virtual bool SetLayerCollision(std::uint8_t first, std::uint8_t second, bool enabled) = 0;
    /**
     * @brief TODO: Describe RayCast.
     *
     * @param origin TODO: Describe this parameter.
     * @param displacement TODO: Describe this parameter.
     * @param hit TODO: Describe this parameter.
     * @param layer_mask TODO: Describe this parameter.
     * @param ignore TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    virtual bool RayCast(const glm::vec3 &origin, const glm::vec3 &displacement, PhysicsQueryHit &hit,
                         std::uint32_t layer_mask, PhysicsBodyHandle ignore) const = 0;
    /**
     * @brief TODO: Describe RayCastAll.
     *
     * @param origin TODO: Describe this parameter.
     * @param displacement TODO: Describe this parameter.
     * @param hits TODO: Describe this parameter.
     * @param capacity TODO: Describe this parameter.
     * @param layer_mask TODO: Describe this parameter.
     * @param ignore TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    virtual std::size_t RayCastAll(const glm::vec3 &origin, const glm::vec3 &displacement, PhysicsQueryHit *hits,
                                   std::size_t capacity, std::uint32_t layer_mask, PhysicsBodyHandle ignore) const = 0;
    /**
     * @brief TODO: Describe ShapeCast.
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
    virtual bool ShapeCast(const PhysicsShape &shape, const glm::vec3 &position, const glm::quat &rotation,
                           const glm::vec3 &displacement, PhysicsQueryHit &hit, std::uint32_t layer_mask,
                           PhysicsBodyHandle ignore) const = 0;
    /**
     * @brief TODO: Describe Overlap.
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
    virtual std::size_t Overlap(const PhysicsShape &shape, const glm::vec3 &position, const glm::quat &rotation,
                                PhysicsBodyHandle *bodies, std::size_t capacity, std::uint32_t layer_mask,
                                PhysicsBodyHandle ignore) const = 0;
    /**
     * @brief TODO: Describe DrainContactEvents.
     *
     * @param events TODO: Describe this parameter.
     * @param capacity TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    virtual std::size_t DrainContactEvents(PhysicsContactEvent *events, std::size_t capacity) = 0;
    /**
     * @brief TODO: Describe PendingContactEventCount.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] virtual std::size_t PendingContactEventCount() const = 0;
    /**
     * @brief TODO: Describe DroppedContactEventCount.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] virtual std::uint64_t DroppedContactEventCount() const = 0;

    /**
     * @brief TODO: Describe CreateConstraint.
     *
     * @param desc TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    virtual PhysicsConstraintHandle CreateConstraint(const PhysicsConstraintDesc &desc) = 0;
    /**
     * @brief TODO: Describe DestroyConstraint.
     *
     * @param constraint TODO: Describe this parameter.
     */
    virtual void DestroyConstraint(PhysicsConstraintHandle constraint) = 0;
    /**
     * @brief TODO: Describe SetConstraintEnabled.
     *
     * @param constraint TODO: Describe this parameter.
     * @param enabled TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    virtual bool SetConstraintEnabled(PhysicsConstraintHandle constraint, bool enabled) = 0;
    /**
     * @brief TODO: Describe SetConstraintMotor.
     *
     * @param constraint TODO: Describe this parameter.
     * @param mode TODO: Describe this parameter.
     * @param target TODO: Describe this parameter.
     * @param force_limit TODO: Describe this parameter.
     * @param frequency TODO: Describe this parameter.
     * @param damping TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    virtual bool SetConstraintMotor(PhysicsConstraintHandle constraint, PhysicsMotorMode mode, float target,
                                    float force_limit, float frequency, float damping) = 0;
    /**
     * @brief TODO: Describe IsValid.
     *
     * @param constraint TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] virtual bool IsValid(PhysicsConstraintHandle constraint) const = 0;
    /**
     * @brief TODO: Describe ConstraintCount.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] virtual std::size_t ConstraintCount() const = 0;

    /**
     * @brief TODO: Describe CreateCharacter.
     *
     * @param desc TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    virtual PhysicsCharacterHandle CreateCharacter(const PhysicsCharacterDesc &desc) = 0;
    /**
     * @brief TODO: Describe DestroyCharacter.
     *
     * @param character TODO: Describe this parameter.
     */
    virtual void DestroyCharacter(PhysicsCharacterHandle character) = 0;
    /**
     * @brief TODO: Describe IsValid.
     *
     * @param character TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] virtual bool IsValid(PhysicsCharacterHandle character) const = 0;
    /**
     * @brief TODO: Describe SetCharacterVelocity.
     *
     * @param character TODO: Describe this parameter.
     * @param velocity TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    virtual bool SetCharacterVelocity(PhysicsCharacterHandle character, const glm::vec3 &velocity) = 0;
    /**
     * @brief TODO: Describe UpdateCharacter.
     *
     * @param character TODO: Describe this parameter.
     * @param fixed_delta_time TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    virtual bool UpdateCharacter(PhysicsCharacterHandle character, float fixed_delta_time) = 0;
    /**
     * @brief TODO: Describe SetCharacterHeight.
     *
     * @param character TODO: Describe this parameter.
     * @param height TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    virtual bool SetCharacterHeight(PhysicsCharacterHandle character, float height) = 0;
    /**
     * @brief TODO: Describe GetCharacterState.
     *
     * @param character TODO: Describe this parameter.
     * @param state TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    virtual bool GetCharacterState(PhysicsCharacterHandle character, PhysicsCharacterState &state) const = 0;
    /**
     * @brief TODO: Describe CharacterCount.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] virtual std::size_t CharacterCount() const = 0;
    /**
     * @brief TODO: Describe BodyCount.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] virtual std::size_t BodyCount() const = 0;
};

#endif
