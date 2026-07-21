#ifndef RENDERABLE_H
#define RENDERABLE_H

#include "renderer/mesh.h"
#include "renderer/lightmap.h"

#include <array>
#include <cstdint>
#include <limits>

#include <glm/glm.hpp>

namespace CEngine::Renderer {

struct RenderableHandle
{
	std::uint32_t index = std::numeric_limits<std::uint32_t>::max();
	std::uint32_t generation = 0;

	constexpr explicit operator bool() const
	{
		return index != std::numeric_limits<std::uint32_t>::max();
	}
};

constexpr bool operator==(RenderableHandle left, RenderableHandle right)
{
	return left.index == right.index && left.generation == right.generation;
}

constexpr bool operator!=(RenderableHandle left, RenderableHandle right)
{
	return !(left == right);
}

enum RenderableFlags : uint32_t
{
	RenderableFlagNone = 0,
	RenderableFlagStatic = 1u << 0u,
	RenderableFlagDynamic = 1u << 1u,
	RenderableFlagCastsShadow = 1u << 2u,
	RenderableFlagReceivesShadow = 1u << 3u
};

struct Renderable
{
	const Mesh* mesh = nullptr;
	Material* material = nullptr;
	const Lightmap* lightmap = nullptr;
	glm::vec2 lightmap_scale = glm::vec2(1.0f);
	glm::vec2 lightmap_offset = glm::vec2(0.0f);
	float lightmap_rgbm_range = 8.0f;
	glm::mat4 transform = glm::mat4(1.0f);
	Bounds local_bounds;
	Bounds world_bounds;
	uint32_t flags = RenderableFlagStatic | RenderableFlagCastsShadow | RenderableFlagReceivesShadow;
};

struct Frustum
{
	std::array<glm::vec4, 6> planes {};
};

Bounds CalculateBounds(const std::vector<glm::vec3>& vertices);
Bounds MergeBounds(const Bounds& first, const Bounds& second);
Bounds TransformBounds(const Bounds& bounds, const glm::mat4& transform);
Frustum ExtractFrustum(const glm::mat4& view_projection);
bool IntersectsFrustum(const Bounds& bounds, const Frustum& frustum);

} // namespace CEngine::Renderer

#endif
