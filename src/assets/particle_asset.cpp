//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/particle_asset.cpp
 * @brief Validates generated particle data.
 * @author Erik Coltey
 */

#include "assets/particle_asset.h"

#include "log.h"

namespace CEngine::Assets
{

bool LoadParticleAsset(const std::filesystem::path &path, Particle &particle)
{
    Particle decoded;
    if (!Decode(path, Type::Particle, decoded) || decoded.lifetime[0] > decoded.lifetime[1] ||
        decoded.speed[0] > decoded.speed[1] || decoded.size[0] < 0.0f || decoded.size[1] < 0.0f ||
        (!decoded.textures.empty() &&
         decoded.textures.front().type != static_cast<std::uint32_t>(Type::Texture)))
    {
        Logging::Logger::Get().Error("assets", "particle payload is invalid");
        return false;
    }
    particle = std::move(decoded);
    return true;
}

} // namespace CEngine::Assets
