// Copyright (c) CEngine contributors.

#include "animation/animation_system.h"

#include "animation/animation_backend.h"
#include "animation/ozz/ozz_animation_backend.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace CEngine::Animations
{
namespace
{
double StartTime(const Assets::Animation &clip, const AnimationPlayback &playback)
{
    return static_cast<double>(clip.Duration()) * static_cast<double>(playback.normalized_start);
}

float SampleRatio(const Assets::Animation &clip, const AnimationPlayback &playback, double time)
{
    const double duration = clip.Duration();
    return playback.looping ? static_cast<float>(std::fmod(time, duration) / duration)
                            : static_cast<float>(std::clamp(time / duration, 0.0, 1.0));
}
} // namespace

struct AnimationSystem::Instance
{
    explicit Instance(std::shared_ptr<const Assets::Skeleton> source) : skeleton(std::move(source))
    {
    }

    std::shared_ptr<const Assets::Skeleton> skeleton;
    std::shared_ptr<const Assets::Animation> current_clip;
    std::shared_ptr<const Assets::Animation> destination_clip;
    AnimationPlayback current_playback;
    AnimationPlayback destination_playback;
    double current_time = 0.0;
    double destination_time = 0.0;
    float fade_elapsed = 0.0f;
    float fade_duration = 0.0f;
    bool transition = false;
    bool frozen_source = false;
    bool paused = false;
};

struct AnimationSystem::Slot
{
    std::uint32_t generation = 1;
    std::unique_ptr<Instance> instance;
};

AnimationSystem::AnimationSystem(AnimationSystemDesc desc)
    : AnimationSystem(std::make_unique<Ozz::AnimationBackend>(), desc)
{
}

AnimationSystem::AnimationSystem(std::unique_ptr<IAnimationBackend> backend, AnimationSystemDesc desc)
    : desc_(desc), backend_(std::move(backend))
{
    events_.reserve(desc_.max_events);
    slots_.reserve(desc_.max_instances);
    free_slots_.reserve(desc_.max_instances);
    if (backend_ == nullptr || desc_.max_instances == 0u || desc_.max_pose_joints == 0u ||
        !backend_->Initialize(desc_))
    {
        ++diagnostics_.backend_failures;
    }
}

AnimationSystem::~AnimationSystem()
{
    if (backend_ != nullptr)
    {
        backend_->Shutdown();
    }
}

AnimationInstanceHandle AnimationSystem::CreateInstance(const AnimationInstanceDesc &desc)
{
    if (backend_ == nullptr || desc.skeleton == nullptr || desc.skeleton->BoneCount() == 0u)
    {
        ++diagnostics_.rejected_commands;
        return {};
    }
    const std::uint32_t joint_count = desc.skeleton->BoneCount();
    if (diagnostics_.live_instances >= desc_.max_instances || joint_count > desc_.max_pose_joints ||
        diagnostics_.pose_joints > desc_.max_pose_joints - joint_count)
    {
        ++diagnostics_.rejected_commands;
        return {};
    }

    std::uint32_t index = 0;
    if (free_slots_.empty())
    {
        index = static_cast<std::uint32_t>(slots_.size());
        slots_.push_back({});
    }
    else
    {
        index = free_slots_.back();
        free_slots_.pop_back();
    }
    if (!backend_->CreateInstance(index, desc.skeleton))
    {
        free_slots_.push_back(index);
        ++diagnostics_.rejected_commands;
        diagnostics_.backend_failures = backend_->RuntimeFailureCount();
        return {};
    }

    Slot &slot = slots_[index];
    slot.instance = std::make_unique<Instance>(desc.skeleton);
    ++diagnostics_.live_instances;
    diagnostics_.high_water_instances =
        std::max(diagnostics_.high_water_instances, diagnostics_.live_instances);
    diagnostics_.pose_joints += joint_count;
    diagnostics_.high_water_pose_joints =
        std::max(diagnostics_.high_water_pose_joints, diagnostics_.pose_joints);
    return {index, slot.generation};
}

bool AnimationSystem::DestroyInstance(AnimationInstanceHandle handle)
{
    Instance *instance = Resolve(handle);
    if (instance == nullptr)
    {
        ++diagnostics_.rejected_commands;
        return false;
    }
    diagnostics_.pose_joints -= instance->skeleton->BoneCount();
    --diagnostics_.live_instances;
    backend_->DestroyInstance(handle.Index());
    Slot &slot = slots_[handle.Index()];
    slot.instance.reset();
    ++slot.generation;
    if (slot.generation == 0u)
    {
        ++slot.generation;
    }
    free_slots_.push_back(handle.Index());
    return true;
}

bool AnimationSystem::ValidateClip(const Instance &instance,
                                   const std::shared_ptr<const Assets::Animation> &clip,
                                   const AnimationPlayback &playback)
{
    if (clip == nullptr || !std::isfinite(playback.rate) || playback.rate < 0.0f ||
        !std::isfinite(playback.normalized_start) || playback.normalized_start < 0.0f ||
        playback.normalized_start > 1.0f ||
        clip->SkeletonReference().guid != instance.skeleton->Identity() ||
        clip->TrackCount() != instance.skeleton->BoneCount())
    {
        ++diagnostics_.rejected_commands;
        return false;
    }
    return true;
}

bool AnimationSystem::Play(AnimationInstanceHandle handle,
                           std::shared_ptr<const Assets::Animation> clip,
                           const AnimationPlayback &playback)
{
    Instance *instance = Resolve(handle);
    if (instance == nullptr || !ValidateClip(*instance, clip, playback))
    {
        if (instance == nullptr)
        {
            ++diagnostics_.rejected_commands;
        }
        return false;
    }
    instance->current_clip = std::move(clip);
    instance->current_playback = playback;
    instance->current_time = StartTime(*instance->current_clip, playback);
    instance->destination_clip.reset();
    instance->transition = false;
    instance->frozen_source = false;
    instance->fade_elapsed = 0.0f;
    instance->fade_duration = 0.0f;
    return true;
}

bool AnimationSystem::CrossFade(AnimationInstanceHandle handle,
                                std::shared_ptr<const Assets::Animation> clip,
                                float duration_seconds,
                                const AnimationPlayback &playback)
{
    if (!std::isfinite(duration_seconds) || duration_seconds < 0.0f)
    {
        ++diagnostics_.rejected_commands;
        return false;
    }
    if (duration_seconds == 0.0f)
    {
        return Play(handle, std::move(clip), playback);
    }
    Instance *instance = Resolve(handle);
    if (instance == nullptr || !ValidateClip(*instance, clip, playback))
    {
        if (instance == nullptr)
        {
            ++diagnostics_.rejected_commands;
        }
        return false;
    }

    if (instance->transition)
    {
        if (!backend_->FreezeSourcePose(handle.Index()))
        {
            ++diagnostics_.rejected_commands;
            diagnostics_.backend_failures = backend_->RuntimeFailureCount();
            return false;
        }
        instance->current_clip.reset();
        instance->frozen_source = true;
    }
    else
    {
        instance->frozen_source = false;
    }

    instance->destination_clip = std::move(clip);
    instance->destination_playback = playback;
    instance->destination_time = StartTime(*instance->destination_clip, playback);
    instance->fade_elapsed = 0.0f;
    instance->fade_duration = duration_seconds;
    instance->transition = true;
    return true;
}

bool AnimationSystem::SetPaused(AnimationInstanceHandle handle, bool paused)
{
    Instance *instance = Resolve(handle);
    if (instance == nullptr)
    {
        ++diagnostics_.rejected_commands;
        return false;
    }
    instance->paused = paused;
    return true;
}

void AnimationSystem::Evaluate(float presentation_delta_seconds)
{
    events_.clear();
    diagnostics_.evaluated_instances = 0;
    diagnostics_.evaluated_joints = 0;
    diagnostics_.active_cross_fades = 0;
    if (!std::isfinite(presentation_delta_seconds) || presentation_delta_seconds < 0.0f)
    {
        ++diagnostics_.rejected_commands;
        return;
    }

    for (std::uint32_t index = 0; index < slots_.size(); ++index)
    {
        Slot &slot = slots_[index];
        if (slot.instance == nullptr)
        {
            continue;
        }
        Instance &instance = *slot.instance;
        const AnimationInstanceHandle handle{index, slot.generation};
        const float delta = instance.paused ? 0.0f : presentation_delta_seconds;
        const float transition_frame_weight =
            instance.transition
                ? std::min((instance.fade_elapsed + delta) / instance.fade_duration, 1.0f)
                : 0.0f;

        AnimationPoseRequest pose;
        pose.sample_source = !instance.frozen_source;
        pose.source_clip = instance.current_clip.get();
        if (instance.current_clip != nullptr && !instance.frozen_source)
        {
            const double previous = instance.current_time;
            instance.current_time += static_cast<double>(delta) * instance.current_playback.rate;
            if (!instance.current_playback.looping)
            {
                instance.current_time =
                    std::min(instance.current_time,
                             static_cast<double>(instance.current_clip->Duration()));
            }
            if (delta > 0.0f &&
                (!instance.transition || transition_frame_weight < 0.5f))
            {
                EmitEvents(handle, *instance.current_clip, previous, instance.current_time,
                           instance.current_playback);
            }
            pose.source_ratio =
                SampleRatio(*instance.current_clip, instance.current_playback, instance.current_time);
        }

        if (instance.transition)
        {
            ++diagnostics_.active_cross_fades;
            const double previous = instance.destination_time;
            instance.destination_time +=
                static_cast<double>(delta) * instance.destination_playback.rate;
            if (!instance.destination_playback.looping)
            {
                instance.destination_time =
                    std::min(instance.destination_time,
                             static_cast<double>(instance.destination_clip->Duration()));
            }
            instance.fade_elapsed =
                std::min(instance.fade_elapsed + delta, instance.fade_duration);
            if (delta > 0.0f && transition_frame_weight >= 0.5f)
            {
                EmitEvents(handle, *instance.destination_clip, previous,
                           instance.destination_time, instance.destination_playback);
            }
            pose.destination_clip = instance.destination_clip.get();
            pose.destination_ratio =
                SampleRatio(*instance.destination_clip, instance.destination_playback,
                            instance.destination_time);
            pose.destination_weight = transition_frame_weight;
        }

        if (!backend_->Evaluate(index, pose))
        {
            ++diagnostics_.rejected_commands;
            continue;
        }

        if (instance.transition && instance.fade_elapsed >= instance.fade_duration)
        {
            instance.current_clip = std::move(instance.destination_clip);
            instance.current_playback = instance.destination_playback;
            instance.current_time = instance.destination_time;
            instance.transition = false;
            instance.frozen_source = false;
        }
        ++diagnostics_.evaluated_instances;
        diagnostics_.evaluated_joints += instance.skeleton->BoneCount();
    }
    diagnostics_.backend_failures = backend_->RuntimeFailureCount();
}

void AnimationSystem::EmitEvents(AnimationInstanceHandle handle,
                                 const Assets::Animation &clip, double previous,
                                 double current, const AnimationPlayback &playback)
{
    if (current <= previous || clip.Markers().empty())
    {
        return;
    }
    const double duration = clip.Duration();
    std::uint64_t total = 0;
    for (const Assets::AnimationMarker &marker : clip.Markers())
    {
        if (playback.looping)
        {
            const double before = std::floor((previous - marker.time) / duration);
            const double after = std::floor((current - marker.time) / duration);
            if (after > before)
            {
                total += static_cast<std::uint64_t>(after - before);
            }
        }
        else if (previous < marker.time && marker.time <= current)
        {
            ++total;
        }
    }

    const std::size_t available =
        events_.size() < desc_.max_events ? desc_.max_events - events_.size() : 0u;
    std::size_t emitted = 0;
    if (available > 0u)
    {
        const std::int64_t first_loop =
            playback.looping
                ? static_cast<std::int64_t>(std::floor(previous / duration))
                : 0;
        const std::int64_t last_loop =
            playback.looping
                ? static_cast<std::int64_t>(std::floor(current / duration))
                : 0;
        for (std::int64_t loop = first_loop;
             loop <= last_loop && emitted < available; ++loop)
        {
            for (const Assets::AnimationMarker &marker : clip.Markers())
            {
                const double occurrence = static_cast<double>(loop) * duration + marker.time;
                if (previous < occurrence && occurrence <= current)
                {
                    events_.push_back(
                        {handle, &clip, marker.id, marker.name, marker.time});
                    ++emitted;
                    if (emitted == available)
                    {
                        break;
                    }
                }
            }
            if (!playback.looping)
            {
                break;
            }
        }
    }
    diagnostics_.dropped_events += total - std::min<std::uint64_t>(total, emitted);
}

std::span<const glm::mat4>
AnimationSystem::SkinningPalette(AnimationInstanceHandle handle) const
{
    return Resolve(handle) == nullptr
               ? std::span<const glm::mat4>{}
               : backend_->SkinningPalette(handle.Index());
}

std::optional<glm::mat4>
AnimationSystem::ModelJoint(AnimationInstanceHandle handle, std::uint16_t joint) const
{
    return Resolve(handle) == nullptr ? std::nullopt
                                      : backend_->ModelJoint(handle.Index(), joint);
}

AnimationSystem::Instance *AnimationSystem::Resolve(AnimationInstanceHandle handle)
{
    return handle && handle.Index() < slots_.size() &&
                   slots_[handle.Index()].generation == handle.Generation()
               ? slots_[handle.Index()].instance.get()
               : nullptr;
}

const AnimationSystem::Instance *
AnimationSystem::Resolve(AnimationInstanceHandle handle) const
{
    return handle && handle.Index() < slots_.size() &&
                   slots_[handle.Index()].generation == handle.Generation()
               ? slots_[handle.Index()].instance.get()
               : nullptr;
}

} // namespace CEngine::Animations
