//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/animation/ozz/ozz_animation_backend.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_ANIMATION_OZZ_OZZ_ANIMATION_BACKEND_H
#define CENGINE_ANIMATION_OZZ_OZZ_ANIMATION_BACKEND_H

#include "animation/animation_backend.h"

#include <memory>

namespace CEngine::Animations::Ozz
{

class AnimationBackend final : public IAnimationBackend
{
  public:
    AnimationBackend();
    ~AnimationBackend() override;
    AnimationBackend(const AnimationBackend &) = delete;
    AnimationBackend &operator=(const AnimationBackend &) = delete;
    AnimationBackend(AnimationBackend &&) = delete;
    AnimationBackend &operator=(AnimationBackend &&) = delete;

    bool Initialize(const AnimationSystemDesc &desc) override;
    void Shutdown() override;
    bool CreateInstance(std::uint32_t slot, std::shared_ptr<const Assets::Skeleton> skeleton) override;
    void DestroyInstance(std::uint32_t slot) override;
    bool FreezeSourcePose(std::uint32_t slot) override;
    bool Evaluate(std::uint32_t slot, const AnimationPoseRequest &request) override;
    [[nodiscard]] std::span<const glm::mat4> SkinningPalette(std::uint32_t slot) const override;
    [[nodiscard]] std::optional<glm::mat4> ModelJoint(std::uint32_t slot,
                                                     std::uint16_t joint) const override;
    [[nodiscard]] std::uint64_t RuntimeFailureCount() const override;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace CEngine::Animations::Ozz

#endif
