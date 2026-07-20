#include "model.h"
#include "render_system.h"
#include <string>
#include <utility>

Model::Model(
	std::unordered_map<std::string, Mesh> in_meshes,
	std::unordered_map<std::string, Material*> in_mats,
	bool auto_register)
	: meshes(std::move(in_meshes)),
	  mats(std::move(in_mats))
{
	if (auto_register)
	{
		RegisterRenderables(glm::mat4(1.0f));
	}
}

std::vector<RenderableHandle> Model::RegisterRenderables(const glm::mat4& transform, uint32_t flags) const
{
	std::vector<RenderableHandle> handles;
	for (const auto& mesh_it : meshes)
	{
		const Mesh& mesh = mesh_it.second;
		for (const auto& material_it : mesh.GetMaterialMeshData())
		{
			Renderable renderable;
			renderable.mesh = &mesh;
			renderable.material = material_it.first;
			renderable.transform = transform;
			renderable.flags = flags;
			renderable.local_bounds = material_it.second.local_bounds.valid ?
				material_it.second.local_bounds : CalculateBounds(material_it.second.vertices);
			renderable.world_bounds = TransformBounds(renderable.local_bounds, transform);
			handles.push_back(RenderSystem::RegisterRenderable(renderable));
		}
	}

	return handles;
}
