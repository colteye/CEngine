#ifndef PHYSICS_SYSTEM_H
#define PHYSICS_SYSTEM_H

#include "physics/physics_types.h"

#include <cstddef>
#include <memory>

class PhysicsSystem
{
  public:
    PhysicsSystem();
    ~PhysicsSystem();

    PhysicsSystem(const PhysicsSystem &) = delete;
    PhysicsSystem &operator=(const PhysicsSystem &) = delete;
    PhysicsSystem(PhysicsSystem &&) noexcept;
    PhysicsSystem &operator=(PhysicsSystem &&) noexcept;

    bool Initialize(const PhysicsSystemDesc &desc);
    void Shutdown();
    void Step(float fixed_delta_time);

    PhysicsBodyHandle CreateBody(const PhysicsBodyDesc &desc, const PhysicsShape &shape);
    void DestroyBody(PhysicsBodyHandle body);
    [[nodiscard]] bool IsValid(PhysicsBodyHandle body) const;
    bool GetBodyState(PhysicsBodyHandle body, PhysicsBodyState &state) const;
    bool SetBodyTransform(PhysicsBodyHandle body, const glm::vec3 &position, const glm::quat &rotation,
                          bool activate = true);
    bool MoveKinematic(PhysicsBodyHandle body, const glm::vec3 &position, const glm::quat &rotation,
                       float fixed_delta_time);
    bool SetVelocity(PhysicsBodyHandle body, const glm::vec3 &linear, const glm::vec3 &angular);
    bool AddForce(PhysicsBodyHandle body, const glm::vec3 &force);
    bool AddForceAt(PhysicsBodyHandle body, const glm::vec3 &force, const glm::vec3 &world_position);
    bool AddTorque(PhysicsBodyHandle body, const glm::vec3 &torque);
    bool AddImpulse(PhysicsBodyHandle body, const glm::vec3 &impulse);
    bool AddImpulseAt(PhysicsBodyHandle body, const glm::vec3 &impulse, const glm::vec3 &world_position);
    bool SetGravityFactor(PhysicsBodyHandle body, float factor);
    bool SetSensor(PhysicsBodyHandle body, bool sensor);
    bool Activate(PhysicsBodyHandle body);
    bool SetLayerCollision(std::uint8_t first, std::uint8_t second, bool enabled);
    bool RayCast(const glm::vec3 &origin, const glm::vec3 &displacement, PhysicsQueryHit &hit,
                 std::uint32_t layer_mask = ~0u, PhysicsBodyHandle ignore = {}) const;
    std::size_t RayCastAll(const glm::vec3 &origin, const glm::vec3 &displacement, PhysicsQueryHit *hits,
                           std::size_t capacity, std::uint32_t layer_mask = ~0u, PhysicsBodyHandle ignore = {}) const;
    bool ShapeCast(const PhysicsShape &shape, const glm::vec3 &position, const glm::quat &rotation,
                   const glm::vec3 &displacement, PhysicsQueryHit &hit, std::uint32_t layer_mask = ~0u,
                   PhysicsBodyHandle ignore = {}) const;
    std::size_t Overlap(const PhysicsShape &shape, const glm::vec3 &position, const glm::quat &rotation,
                        PhysicsBodyHandle *bodies, std::size_t capacity, std::uint32_t layer_mask = ~0u,
                        PhysicsBodyHandle ignore = {}) const;
    std::size_t DrainContactEvents(PhysicsContactEvent *events, std::size_t capacity);
    [[nodiscard]] std::size_t PendingContactEventCount() const;
    [[nodiscard]] std::uint64_t DroppedContactEventCount() const;
    PhysicsConstraintHandle CreateConstraint(const PhysicsConstraintDesc &desc);
    void DestroyConstraint(PhysicsConstraintHandle constraint);
    bool SetConstraintEnabled(PhysicsConstraintHandle constraint, bool enabled);
    bool SetConstraintMotor(PhysicsConstraintHandle constraint, PhysicsMotorMode mode, float target, float force_limit,
                            float frequency = 2.0f, float damping = 1.0f);
    [[nodiscard]] bool IsValid(PhysicsConstraintHandle constraint) const;
    [[nodiscard]] std::size_t ConstraintCount() const;
    PhysicsCharacterHandle CreateCharacter(const PhysicsCharacterDesc &desc);
    void DestroyCharacter(PhysicsCharacterHandle character);
    [[nodiscard]] bool IsValid(PhysicsCharacterHandle character) const;
    bool SetCharacterVelocity(PhysicsCharacterHandle character, const glm::vec3 &velocity);
    bool UpdateCharacter(PhysicsCharacterHandle character, float fixed_delta_time);
    bool SetCharacterHeight(PhysicsCharacterHandle character, float height);
    bool GetCharacterState(PhysicsCharacterHandle character, PhysicsCharacterState &state) const;
    [[nodiscard]] std::size_t CharacterCount() const;
    [[nodiscard]] std::size_t BodyCount() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

#endif
