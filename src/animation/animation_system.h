//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/animation/animation_system.h
 * @brief Client-side skeletal clip playback, blending, events, and poses.
 * @author Erik Coltey
 */

#ifndef CENGINE_ANIMATION_ANIMATION_SYSTEM_H
#define CENGINE_ANIMATION_ANIMATION_SYSTEM_H

#include "assets/animation_asset.h"
#include "assets/skeleton_asset.h"
#include "handle.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <glm/mat4x4.hpp>

namespace CEngine::Animations
{

class IAnimationBackend;

struct AnimationInstanceSlotTag;
using AnimationInstanceHandle = Handle<AnimationInstanceSlotTag>;

struct AnimationSystemDesc
{
    std::uint32_t max_instances = 1024;
    std::uint32_t max_events = 4096;
    std::uint32_t max_pose_joints = 65536;
};

struct AnimationInstanceDesc
{
    std::shared_ptr<const Assets::Skeleton> skeleton;
};

struct AnimationPlayback
{
    float rate = 1.0f;
    float normalized_start = 0.0f;
    bool looping = true;
};

struct AnimationEvent
{
    AnimationInstanceHandle instance;
    const Assets::Animation *clip = nullptr;
    std::uint64_t id = 0;
    std::string_view name;
    float clip_time = 0.0f;
};

struct AnimationDiagnostics
{
    std::uint32_t live_instances = 0;
    std::uint32_t high_water_instances = 0;
    std::uint32_t evaluated_instances = 0;
    std::uint32_t evaluated_joints = 0;
    std::uint32_t active_cross_fades = 0;
    std::uint64_t dropped_events = 0;
    std::uint64_t rejected_commands = 0;
    std::uint64_t backend_failures = 0;
    std::uint32_t pose_joints = 0;
    std::uint32_t high_water_pose_joints = 0;
};

class AnimationSystem
{
  public:
    AnimationSystem();
    explicit AnimationSystem(std::unique_ptr<IAnimationBackend> backend);
    ~AnimationSystem();
    AnimationSystem(const AnimationSystem &) = delete;
    AnimationSystem &operator=(const AnimationSystem &) = delete;
    AnimationSystem(AnimationSystem &&) = delete;
    AnimationSystem &operator=(AnimationSystem &&) = delete;

    bool Initialize(const AnimationSystemDesc &desc = {});
    void Shutdown();
    AnimationInstanceHandle CreateInstance(const AnimationInstanceDesc &desc);
    bool DestroyInstance(AnimationInstanceHandle handle);
    bool Play(AnimationInstanceHandle handle, std::shared_ptr<const Assets::Animation> clip,
              const AnimationPlayback &playback = {});
    bool CrossFade(AnimationInstanceHandle handle, std::shared_ptr<const Assets::Animation> clip,
                   float duration_seconds, const AnimationPlayback &playback = {});
    bool SetPaused(AnimationInstanceHandle handle, bool paused);
    void Evaluate(float presentation_delta_seconds);

    [[nodiscard]] std::span<const AnimationEvent> Events() const
    {
        return events_;
    }
    [[nodiscard]] std::span<const glm::mat4> SkinningPalette(AnimationInstanceHandle handle) const;
    [[nodiscard]] std::optional<glm::mat4> ModelJoint(AnimationInstanceHandle handle,
                                                     std::uint16_t joint) const;
    [[nodiscard]] const AnimationDiagnostics &Diagnostics() const
    {
        return diagnostics_;
    }

  private:
    struct Instance;
    struct Slot;

    [[nodiscard]] Instance *Resolve(AnimationInstanceHandle handle);
    [[nodiscard]] const Instance *Resolve(AnimationInstanceHandle handle) const;
    bool ValidateClip(const Instance &instance, const std::shared_ptr<const Assets::Animation> &clip,
                      const AnimationPlayback &playback);
    void EmitEvents(AnimationInstanceHandle handle, const Assets::Animation &clip, double previous,
                    double current, const AnimationPlayback &playback);

    AnimationSystemDesc desc_;
    std::unique_ptr<IAnimationBackend> backend_;
    std::vector<Slot> slots_;
    std::vector<std::uint32_t> free_slots_;
    std::vector<AnimationEvent> events_;
    AnimationDiagnostics diagnostics_;
    bool initialized_ = false;
};

} // namespace CEngine::Animations

#endif
