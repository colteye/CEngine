//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/texture_asset.h
 * @brief Loads a DDS texture asset into its renderer value.
 * @author Erik Coltey
 */

#ifndef CENGINE_ASSETS_TEXTURE_ASSET_H
#define CENGINE_ASSETS_TEXTURE_ASSET_H

#include "renderer/texture.h"

#include <filesystem>

namespace CEngine::Assets
{

/**
 * @brief TODO: Describe LoadTextureAsset.
 *
 * @param path TODO: Describe this parameter.
 * @param texture TODO: Describe this parameter.
 * @param flip_vertical TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool LoadTextureAsset(const std::filesystem::path &path, Renderer::Texture &texture, bool flip_vertical = true);

} // namespace CEngine::Assets

#endif
