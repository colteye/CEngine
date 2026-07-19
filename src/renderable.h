#ifndef RENDERABLE_H
#define RENDERABLE_H

#include "mesh.h"

#include <cstdint>

#include <glm/glm.hpp>

using RenderableHandle = uint32_t;

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
	glm::mat4 transform = glm::mat4(1.0f);
	Bounds local_bounds;
	Bounds world_bounds;
	uint32_t flags = RenderableFlagStatic | RenderableFlagCastsShadow | RenderableFlagReceivesShadow;
};

Bounds CalculateBounds(const std::vector<glm::vec3>& vertices);
Bounds MergeBounds(const Bounds& first, const Bounds& second);
Bounds TransformBounds(const Bounds& bounds, const glm::mat4& transform);

#endif
