#ifndef MESH_H
#define MESH_H

#include <string>
#include <vector>
#include <unordered_map>

#include <glm/glm.hpp>
#include "renderer/material.h"

namespace CEngine::Renderer {

struct Bounds {
	glm::vec3 min = glm::vec3(0.0f);
	glm::vec3 max = glm::vec3(0.0f);
	bool valid = false;
};

struct MeshData {
	std::vector<glm::vec3> vertices;
	std::vector<glm::vec2> uvs;
	std::vector<glm::vec3> normals;
	std::vector<glm::vec3> tangents;
	Bounds local_bounds;
};

class Mesh
{
public:
	std::string mesh_name;
	void AddMaterialMeshData(Material* material, MeshData mesh_data);

	const std::unordered_map<Material*, MeshData>& GetMaterialMeshData() const { return material_mesh_data; };
	const Bounds& GetLocalBounds() const { return local_bounds; };

private:
	std::unordered_map<Material*, MeshData> material_mesh_data;
	Bounds local_bounds;
};

} // namespace CEngine::Renderer

#endif
