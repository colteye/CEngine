//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/animation/animation_backend.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_ANIMATION_ANIMATION_BACKEND_H
#define CENGINE_ANIMATION_ANIMATION_BACKEND_H

#include <cstdint>
#include <memory>
#include <optional>
#include <span>

#include <glm/mat4x4.hpp>

namespace CEngine::Assets
{
class Animation;
class Skeleton;
}

namespace CEngine::Animations
{

struct AnimationSystemDesc;

struct AnimationPoseRequest
{
    std::shared_ptr<const Assets::Animation> source_clip;
    float source_ratio = 0.0f;
    bool sample_source = true;
    std::shared_ptr<const Assets::Animation> destination_clip;
    float destination_ratio = 0.0f;
    float destination_weight = 0.0f;
};

class IAnimationBackend
{
  public:
    IAnimationBackend() = default;
    virtual ~IAnimationBackend() = default;
    IAnimationBackend(const IAnimationBackend &) = delete;
    IAnimationBackend &operator=(const IAnimationBackend &) = delete;
    IAnimationBackend(IAnimationBackend &&) = delete;
    IAnimationBackend &operator=(IAnimationBackend &&) = delete;

    virtual bool Initialize(const AnimationSystemDesc &desc) = 0;
    virtual void Shutdown() = 0;
    virtual bool CreateInstance(std::uint32_t slot, std::shared_ptr<const Assets::Skeleton> skeleton) = 0;
    virtual void DestroyInstance(std::uint32_t slot) = 0;
    virtual bool FreezeSourcePose(std::uint32_t slot) = 0;
    virtual bool Evaluate(std::uint32_t slot, const AnimationPoseRequest &request) = 0;
    [[nodiscard]] virtual std::span<const glm::mat4> SkinningPalette(std::uint32_t slot) const = 0;
    [[nodiscard]] virtual std::optional<glm::mat4> ModelJoint(std::uint32_t slot,
                                                             std::uint16_t joint) const = 0;
    [[nodiscard]] virtual std::uint64_t RuntimeFailureCount() const = 0;
};

} // namespace CEngine::Animations

#endif
