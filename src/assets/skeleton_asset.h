//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/skeleton_asset.h
 * @brief Immutable decoded skeleton and skin bind data.
 * @author Erik Coltey
 */

#ifndef CENGINE_ASSETS_SKELETON_ASSET_H
#define CENGINE_ASSETS_SKELETON_ASSET_H

#include "assets/format.h"
#include "engine/engine_entities.generated.h"

#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>

#include <glm/mat4x4.hpp>

namespace CEngine::Assets
{

namespace SkeletonWire = Generated::EngineEntities::Wire;
using SkeletonBone = SkeletonWire::SkeletonBone;
using AnimationTransform = SkeletonWire::AnimationTransform;

class Skeleton
{
  public:
    Skeleton() = default;
    ~Skeleton() = default;
    Skeleton(const Skeleton &) = delete;
    Skeleton &operator=(const Skeleton &) = delete;
    Skeleton(Skeleton &&) noexcept;
    Skeleton &operator=(Skeleton &&) noexcept;

    bool Load(const std::filesystem::path &path, const Guid &identity = {});

    [[nodiscard]] std::uint32_t BoneCount() const;
    [[nodiscard]] std::string_view ArmatureName() const;
    [[nodiscard]] const SkeletonBone *Bone(std::uint32_t index) const;
    [[nodiscard]] std::string_view BoneName(std::uint32_t index) const;
    [[nodiscard]] const AnimationTransform *RestTransform(std::uint32_t index) const;
    [[nodiscard]] const glm::mat4 *JointFromArmatureBind(std::uint32_t index) const;
    [[nodiscard]] const Guid &Identity() const
    {
        return identity_;
    }

  private:
    SkeletonWire::Skeleton data_;
    Guid identity_ = {};
    std::vector<glm::mat4> joint_from_armature_bind_;
};

} // namespace CEngine::Assets

#endif
