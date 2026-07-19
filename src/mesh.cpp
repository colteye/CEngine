#include "mesh.h"
#include "renderable.h"

#include <algorithm>
#include <utility>

// material stores set of data from each mesh to render.

void Mesh::AddMaterialMeshData(Material* material, MeshData mesh_data)
{
	if (!mesh_data.local_bounds.valid)
	{
		mesh_data.local_bounds = CalculateBounds(mesh_data.vertices);
	}
	local_bounds = MergeBounds(local_bounds, mesh_data.local_bounds);
	material_mesh_data[material] = std::move(mesh_data);
}
