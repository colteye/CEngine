#include "mesh.h"
#include "render_system.h"

#include <utility>

// material stores set of data from each mesh to render.

void Mesh::AddMaterialMeshData(Material* material, MeshData mesh_data)
{
	material_mesh_data[material] = std::move(mesh_data);
}
