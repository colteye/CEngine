//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/particle_asset.h
 * @brief Generated particle data and its domain validation entry point.
 * @author Erik Coltey
 */

#ifndef CENGINE_ASSETS_PARTICLE_ASSET_H
#define CENGINE_ASSETS_PARTICLE_ASSET_H

#include "engine/engine_entities.generated.h"

#include <filesystem>

namespace CEngine::Assets
{

using Particle = Generated::EngineEntities::Wire::Particle;

bool LoadParticleAsset(const std::filesystem::path &path, Particle &particle);

} // namespace CEngine::Assets

#endif
