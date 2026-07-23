//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/animation_asset.h
 * @brief Owned animation decoded from its generated schema.
 * @author Erik Coltey
 */

#ifndef CENGINE_ASSETS_ANIMATION_ASSET_H
#define CENGINE_ASSETS_ANIMATION_ASSET_H

#include "engine/engine_entities.generated.h"

#include <filesystem>

namespace CEngine::Assets
{

using Animation = Generated::EngineEntities::Wire::Animation;

bool LoadAnimationAsset(const std::filesystem::path &path, Animation &animation);

} // namespace CEngine::Assets

#endif
