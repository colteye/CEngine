#include "mesh.h"
#include "render_system.h"

// material stores set of data from each mesh to render.

void Mesh::addMaterialMeshData(Material * in_mat, 
	std::vector<glm::vec3> in_vertices, 
	std::vector<glm::vec2> in_uvs, 
	std::vector<glm::vec3> in_normals, 
	std::vector<glm::vec3> in_tangents)
{
	MeshData mesh_data = { in_vertices, in_uvs, in_normals, in_tangents };
	material_mesh_data[in_mat] = mesh_data;
}
