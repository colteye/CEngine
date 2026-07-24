//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/mesh_asset.h
 * @brief Loads a cooked mesh asset into its renderer value.
 * @author Erik Coltey
 */

#ifndef CENGINE_MESH_ASSET_H
#define CENGINE_MESH_ASSET_H

#include "renderer/mesh.h"

#include <filesystem>
namespace CEngine::Assets
{

/**
 * @brief TODO: Describe LoadMeshAsset.
 *
 * @param path TODO: Describe this parameter.
 * @param mesh TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool LoadMeshAsset(const std::filesystem::path &path, CEngine::Renderer::Mesh &mesh);

} // namespace CEngine::Assets

#endif
