//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/animation_asset.cpp
 * @brief Validates animation semantics after generated deserialization.
 * @author Erik Coltey
 */

#include "assets/animation_asset.h"

#include "logging/logger.h"

namespace CEngine::Assets
{

bool LoadAnimationAsset(const std::filesystem::path &path, Animation &animation)
{
    Animation decoded;
    if (!Decode(path, Type::Animation, decoded) || decoded.start > decoded.end ||
        decoded.skeleton.type != static_cast<std::uint32_t>(Type::Skeleton))
    {
        Logging::Logger::Get().Error("assets", "animation payload is invalid");
        return false;
    }
    for (const auto &track : decoded.tracks)
    {
        float previous = decoded.start;
        for (const auto &key : track.keys)
        {
            if (key.frame < previous || key.frame < decoded.start || key.frame > decoded.end)
            {
                Logging::Logger::Get().Error("assets", "animation keyframes are out of order");
                return false;
            }
            previous = key.frame;
        }
    }
    animation = std::move(decoded);
    return true;
}

} // namespace CEngine::Assets
