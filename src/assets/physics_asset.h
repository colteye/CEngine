//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/physics_asset.h
 * @brief Loads a cooked collision-shape asset.
 * @author Erik Coltey
 */

#ifndef CENGINE_ASSETS_PHYSICS_ASSET_H
#define CENGINE_ASSETS_PHYSICS_ASSET_H

#include "physics/physics_types.h"

#include <filesystem>

namespace CEngine::Assets
{

/**
 * @brief TODO: Describe LoadPhysicsAsset.
 *
 * @param path TODO: Describe this parameter.
 * @param shape TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool LoadPhysicsAsset(const std::filesystem::path &path, PhysicsShape &shape);

} // namespace CEngine::Assets

#endif
