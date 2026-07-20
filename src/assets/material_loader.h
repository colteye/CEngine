#ifndef CENGINE_MATERIAL_LOADER_H
#define CENGINE_MATERIAL_LOADER_H

#include "renderer/material.h"

#include <filesystem>
#include <string>

namespace CEngine::Assets {

bool LoadMaterialAsset(const std::filesystem::path& path,
    CEngine::Renderer::Material& material,
    std::string* error = nullptr);

} // namespace CEngine::Assets

#endif
