#ifndef CENGINE_MESH_LOADER_H
#define CENGINE_MESH_LOADER_H

#include "renderer/mesh.h"

#include <filesystem>
#include <vector>

namespace CEngine::Assets {

bool LoadMeshAsset(const std::filesystem::path& path,
    const std::vector<CEngine::Renderer::Material*>& material_slots,
    CEngine::Renderer::Mesh& mesh);

} // namespace CEngine::Assets

#endif
