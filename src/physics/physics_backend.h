#ifndef CENGINE_PHYSICS_PHYSICS_BACKEND_H
#define CENGINE_PHYSICS_PHYSICS_BACKEND_H

#include "physics/physics_types.h"

#include <cstddef>
#include <cstdint>

class IPhysicsBackend
{
  public:
    IPhysicsBackend() = default;
    virtual ~IPhysicsBackend() = default;
    IPhysicsBackend(const IPhysicsBackend &) = delete;
    IPhysicsBackend &operator=(const IPhysicsBackend &) = delete;
    IPhysicsBackend(IPhysicsBackend &&) = default;
    IPhysicsBackend &operator=(IPhysicsBackend &&) = default;

    virtual bool Initialize(const PhysicsSystemDesc &desc) = 0;
    virtual void Shutdown() = 0;
    virtual void Step(float fixed_delta_time) = 0;

    virtual PhysicsBodyHandle CreateBody(const PhysicsBodyDesc &desc, const PhysicsShape &shape) = 0;
    virtual void DestroyBody(PhysicsBodyHandle body) = 0;
    [[nodiscard]] virtual bool IsValid(PhysicsBodyHandle body) const = 0;
    virtual bool GetBodyState(PhysicsBodyHandle body, PhysicsBodyState &state) const = 0;
    virtual bool SetBodyTransform(PhysicsBodyHandle body, const glm::vec3 &position, const glm::quat &rotation,
                                  bool activate) = 0;
    virtual bool MoveKinematic(PhysicsBodyHandle body, const glm::vec3 &position, const glm::quat &rotation,
                               float fixed_delta_time) = 0;
    virtual bool SetVelocity(PhysicsBodyHandle body, const glm::vec3 &linear, const glm::vec3 &angular) = 0;
    virtual bool AddForce(PhysicsBodyHandle body, const glm::vec3 &force) = 0;
    virtual bool AddForceAt(PhysicsBodyHandle body, const glm::vec3 &force, const glm::vec3 &world_position) = 0;
    virtual bool AddTorque(PhysicsBodyHandle body, const glm::vec3 &torque) = 0;
    virtual bool AddImpulse(PhysicsBodyHandle body, const glm::vec3 &impulse) = 0;
    virtual bool AddImpulseAt(PhysicsBodyHandle body, const glm::vec3 &impulse, const glm::vec3 &world_position) = 0;
    virtual bool SetGravityFactor(PhysicsBodyHandle body, float factor) = 0;
    virtual bool SetSensor(PhysicsBodyHandle body, bool sensor) = 0;
    virtual bool Activate(PhysicsBodyHandle body) = 0;
    virtual bool SetLayerCollision(std::uint8_t first, std::uint8_t second, bool enabled) = 0;
    virtual bool RayCast(const glm::vec3 &origin, const glm::vec3 &displacement, PhysicsQueryHit &hit,
                         std::uint32_t layer_mask, PhysicsBodyHandle ignore) const = 0;
    virtual std::size_t RayCastAll(const glm::vec3 &origin, const glm::vec3 &displacement, PhysicsQueryHit *hits,
                                   std::size_t capacity, std::uint32_t layer_mask, PhysicsBodyHandle ignore) const = 0;
    virtual bool ShapeCast(const PhysicsShape &shape, const glm::vec3 &position, const glm::quat &rotation,
                           const glm::vec3 &displacement, PhysicsQueryHit &hit, std::uint32_t layer_mask,
                           PhysicsBodyHandle ignore) const = 0;
    virtual std::size_t Overlap(const PhysicsShape &shape, const glm::vec3 &position, const glm::quat &rotation,
                                PhysicsBodyHandle *bodies, std::size_t capacity, std::uint32_t layer_mask,
                                PhysicsBodyHandle ignore) const = 0;
    virtual std::size_t DrainContactEvents(PhysicsContactEvent *events, std::size_t capacity) = 0;
    [[nodiscard]] virtual std::size_t PendingContactEventCount() const = 0;
    [[nodiscard]] virtual std::uint64_t DroppedContactEventCount() const = 0;

    virtual PhysicsConstraintHandle CreateConstraint(const PhysicsConstraintDesc &desc) = 0;
    virtual void DestroyConstraint(PhysicsConstraintHandle constraint) = 0;
    virtual bool SetConstraintEnabled(PhysicsConstraintHandle constraint, bool enabled) = 0;
    virtual bool SetConstraintMotor(PhysicsConstraintHandle constraint, PhysicsMotorMode mode, float target,
                                    float force_limit, float frequency, float damping) = 0;
    [[nodiscard]] virtual bool IsValid(PhysicsConstraintHandle constraint) const = 0;
    [[nodiscard]] virtual std::size_t ConstraintCount() const = 0;

    virtual PhysicsCharacterHandle CreateCharacter(const PhysicsCharacterDesc &desc) = 0;
    virtual void DestroyCharacter(PhysicsCharacterHandle character) = 0;
    [[nodiscard]] virtual bool IsValid(PhysicsCharacterHandle character) const = 0;
    virtual bool SetCharacterVelocity(PhysicsCharacterHandle character, const glm::vec3 &velocity) = 0;
    virtual bool UpdateCharacter(PhysicsCharacterHandle character, float fixed_delta_time) = 0;
    virtual bool SetCharacterHeight(PhysicsCharacterHandle character, float height) = 0;
    virtual bool GetCharacterState(PhysicsCharacterHandle character, PhysicsCharacterState &state) const = 0;
    [[nodiscard]] virtual std::size_t CharacterCount() const = 0;
    [[nodiscard]] virtual std::size_t BodyCount() const = 0;
};

#endif
