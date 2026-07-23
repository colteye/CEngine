#ifndef LIGHT_H
#define LIGHT_H

#include <cstdint>
#include <limits>
#include <glm/glm.hpp>

namespace CEngine::Renderer {

struct LightHandle
{
	std::uint32_t index = std::numeric_limits<std::uint32_t>::max();
	std::uint32_t generation = 0;

	constexpr explicit operator bool() const
	{
		return index != std::numeric_limits<std::uint32_t>::max();
	}
};

constexpr bool operator==(LightHandle left, LightHandle right)
{
	return left.index == right.index && left.generation == right.generation;
}

constexpr bool operator!=(LightHandle left, LightHandle right)
{
	return !(left == right);
}

enum class LightType
{
	Directional = 0,
	Point = 1,
	Spot = 2
};

struct Light
{
	LightType type = LightType::Point;
	glm::vec3 position = glm::vec3(0.0f);
	glm::vec3 direction = glm::vec3(0.0f, 0.0f, -1.0f);
	glm::vec3 color = glm::vec3(1.0f);
	float intensity = 1.0f;
	float range = 0.0f;
	float spot_inner_cos = 0.0f;
	float spot_outer_cos = 0.0f;
	bool enabled = true;
	bool casts_shadows = false;
	uint32_t shadow_resolution = 1024;
	float shadow_bias = 0.0005f;
	float shadow_normal_bias = 0.015f;
};

struct GpuLight
{
	glm::vec4 position_range;
	glm::vec4 direction_spot;
	glm::vec4 color_intensity;
	glm::vec4 params; // x: type, y: spot inner cos, z: shadow index, w: shadow type
};

struct LightShadowGpuHandle
{
	int32_t index = -1;
	int32_t type = 0;

	bool operator==(const LightShadowGpuHandle& other) const
	{
		return index == other.index && type == other.type;
	}
};

} // namespace CEngine::Renderer

#endif
