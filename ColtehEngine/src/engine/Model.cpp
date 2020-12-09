#include "model.h"
#include "render_system.h"
#include <string>

Model::Model(std::vector<glm::vec3>& in_vertices,
	std::vector<glm::vec2>& in_uvs,
	std::vector<glm::vec3>& in_normals,
	std::vector<glm::vec3>& in_tangents)
{
	vertices = in_vertices;
	uvs = in_uvs;
	normals = in_normals;
	tangents = in_tangents;

	RenderSystem::RegisterModel(this);
}