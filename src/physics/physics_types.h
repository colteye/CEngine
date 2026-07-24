//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/physics/physics_types.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef PHYSICS_TYPES_H
#define PHYSICS_TYPES_H

#include "handle.h"

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

struct PhysicsBodySlotTag;
using PhysicsBodyHandle = CEngine::Handle<PhysicsBodySlotTag>;
struct PhysicsConstraintSlotTag;
using PhysicsConstraintHandle = CEngine::Handle<PhysicsConstraintSlotTag>;
struct PhysicsCharacterSlotTag;
using PhysicsCharacterHandle = CEngine::Handle<PhysicsCharacterSlotTag>;

/**
 * @brief TODO: Describe PhysicsMotionType.
 */
enum class PhysicsMotionType
{
    Static,
    Dynamic,
    Kinematic
};

/**
 * @brief TODO: Describe PhysicsShapeType.
 */
enum class PhysicsShapeType
{
    Box,
    Sphere,
    Capsule,
    Cylinder,
    ConvexHull,
    TriangleMesh,
    HeightField,
    Compound,
    Plane
};

// Engine-neutral collision geometry. The same value is produced by the asset
// loader and consumed by PhysicsSystem; no Jolt object crosses this boundary.
/**
 * @brief TODO: Describe PhysicsShape.
 */
struct PhysicsShape
{
    PhysicsShapeType type = PhysicsShapeType::Box;
    glm::vec3 local_position = glm::vec3(0.0f);
    glm::quat local_rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 half_extents = glm::vec3(0.5f);
    float radius = 0.5f;
    float half_height = 0.5f;
    std::vector<glm::vec3> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<float> heights;
    std::uint32_t samples_per_side = 0;
    glm::vec3 height_field_offset = glm::vec3(0.0f);
    glm::vec3 height_field_scale = glm::vec3(1.0f);
    std::vector<PhysicsShape> children;
};

inline constexpr std::uint8_t PhysicsLockTranslationX = 1u << 0u;
inline constexpr std::uint8_t PhysicsLockTranslationY = 1u << 1u;
inline constexpr std::uint8_t PhysicsLockTranslationZ = 1u << 2u;
inline constexpr std::uint8_t PhysicsLockRotationX = 1u << 3u;
inline constexpr std::uint8_t PhysicsLockRotationY = 1u << 4u;
inline constexpr std::uint8_t PhysicsLockRotationZ = 1u << 5u;

/**
 * @brief TODO: Describe PhysicsSystemDesc.
 */
struct PhysicsSystemDesc
{
    glm::vec3 gravity = glm::vec3(0.0f, 0.0f, -9.81f);
    uint32_t max_bodies = 65536;
    uint32_t max_body_pairs = 65536;
    uint32_t max_contact_constraints = 10240;
    uint32_t max_contact_events = 4096;
};

/**
 * @brief TODO: Describe PhysicsBodyDesc.
 */
struct PhysicsBodyDesc
{
    PhysicsMotionType motion_type = PhysicsMotionType::Static;
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    float mass = 1.0f;
    float friction = 0.6f;
    float restitution = 0.05f;
    glm::vec3 linear_velocity = glm::vec3(0.0f);
    glm::vec3 angular_velocity = glm::vec3(0.0f);
    float linear_damping = 0.05f;
    float angular_damping = 0.05f;
    float gravity_factor = 1.0f;
    std::uint8_t locked_axes = 0;
    std::uint8_t collision_layer = 0;
    bool sensor = false;
    bool continuous = false;
    bool allow_sleeping = true;
};

/**
 * @brief TODO: Describe PhysicsBodyState.
 */
struct PhysicsBodyState
{
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 linear_velocity = glm::vec3(0.0f);
    glm::vec3 angular_velocity = glm::vec3(0.0f);
    bool active = false;
};

/**
 * @brief TODO: Describe PhysicsQueryHit.
 */
struct PhysicsQueryHit
{
    PhysicsBodyHandle body;
    PhysicsCharacterHandle character;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 normal = glm::vec3(0.0f);
    float fraction = 0.0f;
    std::uint32_t sub_shape = 0;
};

/**
 * @brief TODO: Describe PhysicsContactType.
 */
enum class PhysicsContactType : std::uint8_t
{
    Begin,
    Persist,
    End
};

/**
 * @brief TODO: Describe PhysicsContactEvent.
 */
struct PhysicsContactEvent
{
    PhysicsContactType type = PhysicsContactType::Begin;
    PhysicsBodyHandle first;
    PhysicsBodyHandle second;
    PhysicsCharacterHandle first_character;
    PhysicsCharacterHandle second_character;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 normal = glm::vec3(0.0f);
    bool sensor = false;
};

/**
 * @brief TODO: Describe PhysicsConstraintType.
 */
enum class PhysicsConstraintType : std::uint8_t
{
    Fixed,
    Point,
    Hinge,
    Slider,
    Distance,
    Cone
};

/**
 * @brief TODO: Describe PhysicsMotorMode.
 */
enum class PhysicsMotorMode : std::uint8_t
{
    Off,
    Velocity,
    Position
};

/**
 * @brief TODO: Describe PhysicsConstraintDesc.
 */
struct PhysicsConstraintDesc
{
    PhysicsConstraintType type = PhysicsConstraintType::Fixed;
    PhysicsBodyHandle first;
    PhysicsBodyHandle second;
    glm::vec3 first_anchor = glm::vec3(0.0f);
    glm::vec3 second_anchor = glm::vec3(0.0f);
    glm::vec3 axis = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 normal = glm::vec3(1.0f, 0.0f, 0.0f);
    float minimum = 0.0f;
    float maximum = 0.0f;
    float spring_frequency = 0.0f;
    float spring_damping = 0.0f;
    PhysicsMotorMode motor = PhysicsMotorMode::Off;
    float motor_target = 0.0f;
    float motor_force_limit = 0.0f;
    float motor_frequency = 2.0f;
    float motor_damping = 1.0f;
    bool enabled = true;
};

/**
 * @brief TODO: Describe PhysicsCharacterDesc.
 */
struct PhysicsCharacterDesc
{
    glm::vec3 position = glm::vec3(0.0f);
    float radius = 0.4f;
    float height = 1.8f;
    float mass = 70.0f;
    float max_strength = 100.0f;
    float max_slope_radians = 0.7853982f;
    float step_height = 0.4f;
    float stick_to_floor_distance = 0.5f;
    float padding = 0.02f;
    float penetration_recovery = 1.0f;
    std::uint8_t collision_layer = 0;
};

/**
 * @brief TODO: Describe PhysicsCharacterState.
 */
struct PhysicsCharacterState
{
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 velocity = glm::vec3(0.0f);
    glm::vec3 ground_normal = glm::vec3(0.0f);
    glm::vec3 ground_velocity = glm::vec3(0.0f);
    PhysicsBodyHandle ground_body;
    bool grounded = false;
    bool slope_too_steep = false;
};

#endif
