//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/skeleton_asset.h
 * @brief Owned runtime representation of a cooked skeleton.
 * @author Erik Coltey
 */

#ifndef CENGINE_ASSETS_SKELETON_ASSET_H
#define CENGINE_ASSETS_SKELETON_ASSET_H

#include "engine/engine_entities.generated.h"

#include <cstdint>
#include <filesystem>
#include <string_view>

namespace CEngine::Assets
{

namespace SkeletonWire = Generated::EngineEntities::Wire;
using SkeletonBone = SkeletonWire::SkeletonBone;

class Skeleton
{
  public:
    bool Load(const std::filesystem::path &path);

    [[nodiscard]] std::uint32_t BoneCount() const
    {
        return static_cast<std::uint32_t>(data_.bones.size());
    }
    [[nodiscard]] std::string_view ArmatureName() const
    {
        return data_.name;
    }
    [[nodiscard]] const SkeletonBone *Bone(std::uint32_t index) const
    {
        return index < data_.bones.size() ? &data_.bones[index] : nullptr;
    }
    [[nodiscard]] std::string_view BoneName(std::uint32_t index) const
    {
        const SkeletonBone *bone = Bone(index);
        return bone == nullptr ? std::string_view{} : std::string_view(bone->name);
    }

  private:
    SkeletonWire::Skeleton data_;
};

} // namespace CEngine::Assets

#endif
