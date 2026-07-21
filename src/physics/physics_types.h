#ifndef PHYSICS_TYPES_H
#define PHYSICS_TYPES_H

#include <cstdint>
#include <limits>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

struct PhysicsBodyHandle
{
	std::uint32_t index = std::numeric_limits<std::uint32_t>::max();
	std::uint32_t generation = 0;

	constexpr explicit operator bool() const
	{
		return index != std::numeric_limits<std::uint32_t>::max();
	}
};

constexpr bool operator==(PhysicsBodyHandle left, PhysicsBodyHandle right)
{
	return left.index == right.index && left.generation == right.generation;
}

constexpr bool operator!=(PhysicsBodyHandle left, PhysicsBodyHandle right)
{
	return !(left == right);
}

constexpr PhysicsBodyHandle kInvalidPhysicsBodyHandle;

enum class PhysicsMotionType
{
	Static,
	Dynamic,
	Kinematic
};

enum class PhysicsShapeType
{
	Box,
	Sphere
};

struct PhysicsSystemDesc
{
	glm::vec3 gravity = glm::vec3(0.0f, 0.0f, -9.81f);
	float fixed_time_step = 1.0f / 60.0f;
	uint32_t max_sub_steps = 4;
	uint32_t max_bodies = 65536;
	uint32_t max_body_pairs = 65536;
	uint32_t max_contact_constraints = 10240;
};

struct PhysicsBodyDesc
{
	PhysicsMotionType motion_type = PhysicsMotionType::Static;
	PhysicsShapeType shape_type = PhysicsShapeType::Box;
	glm::vec3 position = glm::vec3(0.0f);
	glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	glm::vec3 box_half_extents = glm::vec3(0.5f);
	float sphere_radius = 0.5f;
	float mass = 1.0f;
	float friction = 0.6f;
	float restitution = 0.05f;
	glm::vec3 linear_velocity = glm::vec3(0.0f);
	glm::vec3 angular_velocity = glm::vec3(0.0f);
};

#endif
