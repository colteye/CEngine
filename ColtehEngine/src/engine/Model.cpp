#include "model.h"
#include "render_system.h"
#include <string>

Model::Model(
	std::unordered_map<std::string, Mesh>& in_meshes,
	std::unordered_map<std::string, Material*>& in_mats)
{
	meshes = in_meshes;
	mats = in_mats;

	// Register all meshes.
	// Materials should already be registered!
	for (auto& it : meshes)
	{
		RenderSystem::RegisterMesh(&it.second);
	}
}