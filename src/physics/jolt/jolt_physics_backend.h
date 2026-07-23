#ifndef CENGINE_PHYSICS_JOLT_JOLT_PHYSICS_BACKEND_H
#define CENGINE_PHYSICS_JOLT_JOLT_PHYSICS_BACKEND_H

#include "physics/physics_backend.h"

#include <memory>

class JoltPhysicsBackend final : public IPhysicsBackend
{
  public:
    JoltPhysicsBackend();
    ~JoltPhysicsBackend() override;

    JoltPhysicsBackend(const JoltPhysicsBackend &) = delete;
    JoltPhysicsBackend &operator=(const JoltPhysicsBackend &) = delete;
    JoltPhysicsBackend(JoltPhysicsBackend &&) noexcept;
    JoltPhysicsBackend &operator=(JoltPhysicsBackend &&) noexcept;

    bool Initialize(const PhysicsSystemDesc &desc) override;
    void Shutdown() override;
    void Step(float fixed_delta_time) override;

    PhysicsBodyHandle CreateBody(const PhysicsBodyDesc &desc, const PhysicsShape &shape) override;
    void DestroyBody(PhysicsBodyHandle body) override;
    [[nodiscard]] bool IsValid(PhysicsBodyHandle body) const override;
    bool GetBodyState(PhysicsBodyHandle body, PhysicsBodyState &state) const override;
    bool SetBodyTransform(PhysicsBodyHandle body, const glm::vec3 &position, const glm::quat &rotation,
                          bool activate) override;
    bool MoveKinematic(PhysicsBodyHandle body, const glm::vec3 &position, const glm::quat &rotation,
                       float fixed_delta_time) override;
    bool SetVelocity(PhysicsBodyHandle body, const glm::vec3 &linear, const glm::vec3 &angular) override;
    bool AddForce(PhysicsBodyHandle body, const glm::vec3 &force) override;
    bool AddForceAt(PhysicsBodyHandle body, const glm::vec3 &force, const glm::vec3 &world_position) override;
    bool AddTorque(PhysicsBodyHandle body, const glm::vec3 &torque) override;
    bool AddImpulse(PhysicsBodyHandle body, const glm::vec3 &impulse) override;
    bool AddImpulseAt(PhysicsBodyHandle body, const glm::vec3 &impulse, const glm::vec3 &world_position) override;
    bool SetGravityFactor(PhysicsBodyHandle body, float factor) override;
    bool SetSensor(PhysicsBodyHandle body, bool sensor) override;
    bool Activate(PhysicsBodyHandle body) override;
    bool SetLayerCollision(std::uint8_t first, std::uint8_t second, bool enabled) override;
    bool RayCast(const glm::vec3 &origin, const glm::vec3 &displacement, PhysicsQueryHit &hit, std::uint32_t layer_mask,
                 PhysicsBodyHandle ignore) const override;
    std::size_t RayCastAll(const glm::vec3 &origin, const glm::vec3 &displacement, PhysicsQueryHit *hits,
                           std::size_t capacity, std::uint32_t layer_mask, PhysicsBodyHandle ignore) const override;
    bool ShapeCast(const PhysicsShape &shape, const glm::vec3 &position, const glm::quat &rotation,
                   const glm::vec3 &displacement, PhysicsQueryHit &hit, std::uint32_t layer_mask,
                   PhysicsBodyHandle ignore) const override;
    std::size_t Overlap(const PhysicsShape &shape, const glm::vec3 &position, const glm::quat &rotation,
                        PhysicsBodyHandle *bodies, std::size_t capacity, std::uint32_t layer_mask,
                        PhysicsBodyHandle ignore) const override;
    std::size_t DrainContactEvents(PhysicsContactEvent *events, std::size_t capacity) override;
    [[nodiscard]] std::size_t PendingContactEventCount() const override;
    [[nodiscard]] std::uint64_t DroppedContactEventCount() const override;

    PhysicsConstraintHandle CreateConstraint(const PhysicsConstraintDesc &desc) override;
    void DestroyConstraint(PhysicsConstraintHandle constraint) override;
    bool SetConstraintEnabled(PhysicsConstraintHandle constraint, bool enabled) override;
    bool SetConstraintMotor(PhysicsConstraintHandle constraint, PhysicsMotorMode mode, float target, float force_limit,
                            float frequency, float damping) override;
    [[nodiscard]] bool IsValid(PhysicsConstraintHandle constraint) const override;
    [[nodiscard]] std::size_t ConstraintCount() const override;

    PhysicsCharacterHandle CreateCharacter(const PhysicsCharacterDesc &desc) override;
    void DestroyCharacter(PhysicsCharacterHandle character) override;
    [[nodiscard]] bool IsValid(PhysicsCharacterHandle character) const override;
    bool SetCharacterVelocity(PhysicsCharacterHandle character, const glm::vec3 &velocity) override;
    bool UpdateCharacter(PhysicsCharacterHandle character, float fixed_delta_time) override;
    bool SetCharacterHeight(PhysicsCharacterHandle character, float height) override;
    bool GetCharacterState(PhysicsCharacterHandle character, PhysicsCharacterState &state) const override;
    [[nodiscard]] std::size_t CharacterCount() const override;
    [[nodiscard]] std::size_t BodyCount() const override;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

#endif
