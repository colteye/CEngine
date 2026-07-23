#ifndef CENGINE_MESH_LOADER_H
#define CENGINE_MESH_LOADER_H

#include "renderer/mesh.h"

#include <filesystem>
namespace CEngine::Assets
{

bool LoadMeshAsset(const std::filesystem::path &path, CEngine::Renderer::Mesh &mesh);

} // namespace CEngine::Assets

#endif
