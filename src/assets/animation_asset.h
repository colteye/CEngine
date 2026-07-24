//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/animation_asset.h
 * @brief Immutable backend-neutral animation tracks and cosmetic events.
 * @author Erik Coltey
 */

#ifndef CENGINE_ASSETS_ANIMATION_ASSET_H
#define CENGINE_ASSETS_ANIMATION_ASSET_H

#include "assets/format.h"
#include "engine/engine_entities.generated.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace CEngine::Assets
{

namespace AnimationWire = Generated::EngineEntities::Wire;
using AnimationTrack = AnimationWire::AnimationTrack;

struct AnimationMarker
{
    float time = 0.0f;
    std::uint64_t id = 0;
    std::string name;
};

class Animation
{
  public:
    Animation() = default;
    ~Animation() = default;
    Animation(const Animation &) = delete;
    Animation &operator=(const Animation &) = delete;
    Animation(Animation &&) noexcept;
    Animation &operator=(Animation &&) noexcept;

    [[nodiscard]] std::string_view Name() const
    {
        return name_;
    }
    [[nodiscard]] float Duration() const
    {
        return duration_;
    }
    [[nodiscard]] const Reference &SkeletonReference() const
    {
        return skeleton_;
    }
    [[nodiscard]] std::span<const AnimationMarker> Markers() const
    {
        return markers_;
    }
    [[nodiscard]] std::uint32_t TrackCount() const;
    [[nodiscard]] std::span<const AnimationTrack> Tracks() const
    {
        return tracks_;
    }

  private:
    friend bool LoadAnimationAsset(const std::filesystem::path &, Animation &);

    std::string name_;
    Reference skeleton_;
    float duration_ = 0.0f;
    std::vector<AnimationMarker> markers_;
    std::vector<AnimationTrack> tracks_;
};

bool LoadAnimationAsset(const std::filesystem::path &path, Animation &animation);

} // namespace CEngine::Assets

#endif
