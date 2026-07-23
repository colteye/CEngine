#ifndef CENGINE_ASSETS_TEXTURE_LOADER_H
#define CENGINE_ASSETS_TEXTURE_LOADER_H

#include "renderer/texture.h"

#include <filesystem>

namespace CEngine::Assets
{

bool LoadTextureAsset(const std::filesystem::path &path, Renderer::Texture &texture, bool flip_vertical = true);

} // namespace CEngine::Assets

#endif
