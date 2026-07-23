#ifndef CENGINE_MATERIAL_LOADER_H
#define CENGINE_MATERIAL_LOADER_H

#include "renderer/material.h"

#include <filesystem>

namespace CEngine::Assets {

bool LoadMaterialAsset(const std::filesystem::path& path,
    CEngine::Renderer::Material& material);

} // namespace CEngine::Assets

#endif
