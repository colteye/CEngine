//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/material_asset.h
 * @brief Loads a cooked material asset into its renderer value.
 * @author Erik Coltey
 */

#ifndef CENGINE_MATERIAL_ASSET_H
#define CENGINE_MATERIAL_ASSET_H

#include "renderer/material.h"

#include <filesystem>

namespace CEngine::Assets
{

/**
 * @brief TODO: Describe LoadMaterialAsset.
 *
 * @param path TODO: Describe this parameter.
 * @param material TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool LoadMaterialAsset(const std::filesystem::path &path, CEngine::Renderer::Material &material);

} // namespace CEngine::Assets

#endif
