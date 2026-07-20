#include "renderer/renderable.h"

#include <algorithm>
#include <array>

namespace CEngine::Renderer {

Bounds CalculateBounds(const std::vector<glm::vec3>& vertices)
{
	Bounds bounds;
	if (vertices.empty())
	{
		return bounds;
	}

	bounds.min = vertices.front();
	bounds.max = vertices.front();
	bounds.valid = true;

	for (const glm::vec3& vertex : vertices)
	{
		bounds.min = glm::min(bounds.min, vertex);
		bounds.max = glm::max(bounds.max, vertex);
	}

	return bounds;
}

Bounds MergeBounds(const Bounds& first, const Bounds& second)
{
	if (!first.valid)
	{
		return second;
	}
	if (!second.valid)
	{
		return first;
	}

	Bounds merged;
	merged.min = glm::min(first.min, second.min);
	merged.max = glm::max(first.max, second.max);
	merged.valid = true;
	return merged;
}

Bounds TransformBounds(const Bounds& bounds, const glm::mat4& transform)
{
	if (!bounds.valid)
	{
		return bounds;
	}

	const std::array<glm::vec3, 8> corners = {
		glm::vec3(bounds.min.x, bounds.min.y, bounds.min.z),
		glm::vec3(bounds.max.x, bounds.min.y, bounds.min.z),
		glm::vec3(bounds.min.x, bounds.max.y, bounds.min.z),
		glm::vec3(bounds.max.x, bounds.max.y, bounds.min.z),
		glm::vec3(bounds.min.x, bounds.min.y, bounds.max.z),
		glm::vec3(bounds.max.x, bounds.min.y, bounds.max.z),
		glm::vec3(bounds.min.x, bounds.max.y, bounds.max.z),
		glm::vec3(bounds.max.x, bounds.max.y, bounds.max.z),
	};

	Bounds transformed;
	for (const glm::vec3& corner : corners)
	{
		const glm::vec3 world_corner = glm::vec3(transform * glm::vec4(corner, 1.0f));
		if (!transformed.valid)
		{
			transformed.min = world_corner;
			transformed.max = world_corner;
			transformed.valid = true;
		}
		else
		{
			transformed.min = glm::min(transformed.min, world_corner);
			transformed.max = glm::max(transformed.max, world_corner);
		}
	}

	return transformed;
}

} // namespace CEngine::Renderer
