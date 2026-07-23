//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/skeleton_asset.cpp
 * @brief Instantiates an owned skeleton from generated wire data.
 * @author Erik Coltey
 */

#include "assets/skeleton_asset.h"

#include "logging/logger.h"

#include <unordered_set>

namespace CEngine::Assets
{

bool Skeleton::Load(const std::filesystem::path &path)
{
    SkeletonWire::Skeleton decoded;
    if (!Decode(path, Type::Skeleton, decoded))
    {
        Logging::Logger::Get().Error("assets", "skeleton payload is invalid");
        return false;
    }

    std::unordered_set<std::string> names;
    for (std::size_t index = 0; index < decoded.bones.size(); ++index)
    {
        const SkeletonBone &bone = decoded.bones[index];
        if (!names.insert(bone.name).second || (index == 0u && bone.parent != -1) ||
            (index != 0u && (bone.parent < 0 || static_cast<std::size_t>(bone.parent) >= index)))
        {
            Logging::Logger::Get().Error("assets", "skeleton hierarchy is invalid");
            return false;
        }
    }
    data_ = std::move(decoded);
    return true;
}

} // namespace CEngine::Assets
