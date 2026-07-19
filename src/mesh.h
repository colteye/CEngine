#ifndef MESH_H
#define MESH_H

#include <string>
#include <vector>
#include <unordered_map>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include "material.h"

struct MeshData {
	std::vector<glm::vec3> vertices;
	std::vector<glm::vec2> uvs;
	std::vector<glm::vec3> normals;
	std::vector<glm::vec3> tangents;
};

class Mesh
{
public:
	std::string mesh_name;
	void addMaterialMeshData(Material* in_mat, std::vector<glm::vec3> in_vertices,
		std::vector<glm::vec2> in_uvs,
		std::vector<glm::vec3> in_normals,
		std::vector<glm::vec3> in_tangents);

	std::unordered_map<Material*, MeshData> &getMaterialMeshData() { return material_mesh_data; };

private:
	std::unordered_map<Material*, MeshData> material_mesh_data;
};
#endif