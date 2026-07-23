//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/physics/jolt/jolt_physics_backend.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_PHYSICS_JOLT_JOLT_PHYSICS_BACKEND_H
#define CENGINE_PHYSICS_JOLT_JOLT_PHYSICS_BACKEND_H

#include "physics/physics_backend.h"

#include <memory>

/**
 * @brief TODO: Describe JoltPhysicsBackend.
 */
class JoltPhysicsBackend final : public IPhysicsBackend
{
  public:
    /**
     * @brief TODO: Describe JoltPhysicsBackend.
     */
    JoltPhysicsBackend();
    /**
     * @brief TODO: Describe ~JoltPhysicsBackend.
     */
    ~JoltPhysicsBackend() override;

    /**
     * @brief TODO: Describe JoltPhysicsBackend.
     */
    JoltPhysicsBackend(const JoltPhysicsBackend &) = delete;
    /**
     * @brief TODO: Describe operator=.
     *
     * @return TODO: Describe the return value.
     */
    JoltPhysicsBackend &operator=(const JoltPhysicsBackend &) = delete;
    /**
     * @brief TODO: Describe JoltPhysicsBackend.
     */
    JoltPhysicsBackend(JoltPhysicsBackend &&) noexcept;
    /**
     * @brief TODO: Describe operator=.
     *
     * @return TODO: Describe the return value.
     */
    JoltPhysicsBackend &operator=(JoltPhysicsBackend &&) noexcept;

    /**
     * @brief TODO: Describe Initialize.
     *
     * @param desc TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool Initialize(const PhysicsSystemDesc &desc) override;
    /**
     * @brief TODO: Describe Shutdown.
     */
    void Shutdown() override;
    /**
     * @brief TODO: Describe Step.
     *
     * @param fixed_delta_time TODO: Describe this parameter.
     */
    void Step(float fixed_delta_time) override;

    /**
     * @brief TODO: Describe CreateBody.
     *
     * @param desc TODO: Describe this parameter.
     * @param shape TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    PhysicsBodyHandle CreateBody(const PhysicsBodyDesc &desc, const PhysicsShape &shape) override;
    /**
     * @brief TODO: Describe DestroyBody.
     *
     * @param body TODO: Describe this parameter.
     */
    void DestroyBody(PhysicsBodyHandle body) override;
    /**
     * @brief TODO: Describe IsValid.
     *
     * @param body TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] bool IsValid(PhysicsBodyHandle body) const override;
    /**
     * @brief TODO: Describe GetBodyState.
     *
     * @param body TODO: Describe this parameter.
     * @param state TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool GetBodyState(PhysicsBodyHandle body, PhysicsBodyState &state) const override;
    /**
     * @brief TODO: Describe SetBodyTransform.
     *
     * @param body TODO: Describe this parameter.
     * @param position TODO: Describe this parameter.
     * @param rotation TODO: Describe this parameter.
     * @param activate TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool SetBodyTransform(PhysicsBodyHandle body, const glm::vec3 &position, const glm::quat &rotation,
                          bool activate) override;
    /**
     * @brief TODO: Describe MoveKinematic.
     *
     * @param body TODO: Describe this parameter.
     * @param position TODO: Describe this parameter.
     * @param rotation TODO: Describe this parameter.
     * @param fixed_delta_time TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool MoveKinematic(PhysicsBodyHandle body, const glm::vec3 &position, const glm::quat &rotation,
                       float fixed_delta_time) override;
    /**
     * @brief TODO: Describe SetVelocity.
     *
     * @param body TODO: Describe this parameter.
     * @param linear TODO: Describe this parameter.
     * @param angular TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool SetVelocity(PhysicsBodyHandle body, const glm::vec3 &linear, const glm::vec3 &angular) override;
    /**
     * @brief TODO: Describe AddForce.
     *
     * @param body TODO: Describe this parameter.
     * @param force TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool AddForce(PhysicsBodyHandle body, const glm::vec3 &force) override;
    /**
     * @brief TODO: Describe AddForceAt.
     *
     * @param body TODO: Describe this parameter.
     * @param force TODO: Describe this parameter.
     * @param world_position TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool AddForceAt(PhysicsBodyHandle body, const glm::vec3 &force, const glm::vec3 &world_position) override;
    /**
     * @brief TODO: Describe AddTorque.
     *
     * @param body TODO: Describe this parameter.
     * @param torque TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool AddTorque(PhysicsBodyHandle body, const glm::vec3 &torque) override;
    /**
     * @brief TODO: Describe AddImpulse.
     *
     * @param body TODO: Describe this parameter.
     * @param impulse TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool AddImpulse(PhysicsBodyHandle body, const glm::vec3 &impulse) override;
    /**
     * @brief TODO: Describe AddImpulseAt.
     *
     * @param body TODO: Describe this parameter.
     * @param impulse TODO: Describe this parameter.
     * @param world_position TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool AddImpulseAt(PhysicsBodyHandle body, const glm::vec3 &impulse, const glm::vec3 &world_position) override;
    /**
     * @brief TODO: Describe SetGravityFactor.
     *
     * @param body TODO: Describe this parameter.
     * @param factor TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool SetGravityFactor(PhysicsBodyHandle body, float factor) override;
    /**
     * @brief TODO: Describe SetSensor.
     *
     * @param body TODO: Describe this parameter.
     * @param sensor TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool SetSensor(PhysicsBodyHandle body, bool sensor) override;
    /**
     * @brief TODO: Describe Activate.
     *
     * @param body TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool Activate(PhysicsBodyHandle body) override;
    /**
     * @brief TODO: Describe SetLayerCollision.
     *
     * @param first TODO: Describe this parameter.
     * @param second TODO: Describe this parameter.
     * @param enabled TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool SetLayerCollision(std::uint8_t first, std::uint8_t second, bool enabled) override;
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
    bool RayCast(const glm::vec3 &origin, const glm::vec3 &displacement, PhysicsQueryHit &hit, std::uint32_t layer_mask,
                 PhysicsBodyHandle ignore) const override;
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
    std::size_t RayCastAll(const glm::vec3 &origin, const glm::vec3 &displacement, PhysicsQueryHit *hits,
                           std::size_t capacity, std::uint32_t layer_mask, PhysicsBodyHandle ignore) const override;
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
    bool ShapeCast(const PhysicsShape &shape, const glm::vec3 &position, const glm::quat &rotation,
                   const glm::vec3 &displacement, PhysicsQueryHit &hit, std::uint32_t layer_mask,
                   PhysicsBodyHandle ignore) const override;
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
    std::size_t Overlap(const PhysicsShape &shape, const glm::vec3 &position, const glm::quat &rotation,
                        PhysicsBodyHandle *bodies, std::size_t capacity, std::uint32_t layer_mask,
                        PhysicsBodyHandle ignore) const override;
    /**
     * @brief TODO: Describe DrainContactEvents.
     *
     * @param events TODO: Describe this parameter.
     * @param capacity TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    std::size_t DrainContactEvents(PhysicsContactEvent *events, std::size_t capacity) override;
    /**
     * @brief TODO: Describe PendingContactEventCount.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] std::size_t PendingContactEventCount() const override;
    /**
     * @brief TODO: Describe DroppedContactEventCount.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] std::uint64_t DroppedContactEventCount() const override;

    /**
     * @brief TODO: Describe CreateConstraint.
     *
     * @param desc TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    PhysicsConstraintHandle CreateConstraint(const PhysicsConstraintDesc &desc) override;
    /**
     * @brief TODO: Describe DestroyConstraint.
     *
     * @param constraint TODO: Describe this parameter.
     */
    void DestroyConstraint(PhysicsConstraintHandle constraint) override;
    /**
     * @brief TODO: Describe SetConstraintEnabled.
     *
     * @param constraint TODO: Describe this parameter.
     * @param enabled TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool SetConstraintEnabled(PhysicsConstraintHandle constraint, bool enabled) override;
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
    bool SetConstraintMotor(PhysicsConstraintHandle constraint, PhysicsMotorMode mode, float target, float force_limit,
                            float frequency, float damping) override;
    /**
     * @brief TODO: Describe IsValid.
     *
     * @param constraint TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] bool IsValid(PhysicsConstraintHandle constraint) const override;
    /**
     * @brief TODO: Describe ConstraintCount.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] std::size_t ConstraintCount() const override;

    /**
     * @brief TODO: Describe CreateCharacter.
     *
     * @param desc TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    PhysicsCharacterHandle CreateCharacter(const PhysicsCharacterDesc &desc) override;
    /**
     * @brief TODO: Describe DestroyCharacter.
     *
     * @param character TODO: Describe this parameter.
     */
    void DestroyCharacter(PhysicsCharacterHandle character) override;
    /**
     * @brief TODO: Describe IsValid.
     *
     * @param character TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] bool IsValid(PhysicsCharacterHandle character) const override;
    /**
     * @brief TODO: Describe SetCharacterVelocity.
     *
     * @param character TODO: Describe this parameter.
     * @param velocity TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool SetCharacterVelocity(PhysicsCharacterHandle character, const glm::vec3 &velocity) override;
    /**
     * @brief TODO: Describe UpdateCharacter.
     *
     * @param character TODO: Describe this parameter.
     * @param fixed_delta_time TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool UpdateCharacter(PhysicsCharacterHandle character, float fixed_delta_time) override;
    /**
     * @brief TODO: Describe SetCharacterHeight.
     *
     * @param character TODO: Describe this parameter.
     * @param height TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool SetCharacterHeight(PhysicsCharacterHandle character, float height) override;
    /**
     * @brief TODO: Describe GetCharacterState.
     *
     * @param character TODO: Describe this parameter.
     * @param state TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool GetCharacterState(PhysicsCharacterHandle character, PhysicsCharacterState &state) const override;
    /**
     * @brief TODO: Describe CharacterCount.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] std::size_t CharacterCount() const override;
    /**
     * @brief TODO: Describe BodyCount.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] std::size_t BodyCount() const override;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

#endif
