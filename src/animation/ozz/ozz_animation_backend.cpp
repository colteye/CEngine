// Copyright (c) CEngine contributors.

#include "animation/ozz/ozz_animation_backend.h"

#include "animation/animation_system.h"
#include "assets/animation_asset.h"
#include "assets/skeleton_asset.h"

#include <ozz/animation/offline/animation_builder.h>
#include <ozz/animation/offline/raw_animation.h>
#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/skeleton_builder.h>
#include <ozz/animation/runtime/blending_job.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/base/maths/quaternion.h>
#include <ozz/base/maths/simd_math.h>
#include <ozz/base/maths/soa_transform.h>
#include <ozz/base/maths/transform.h>
#include <ozz/base/span.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace CEngine::Animations::Ozz
{
namespace
{
glm::mat4 Matrix(const ozz::math::Float4x4 &source)
{
    glm::mat4 output(1.0f);
    for (std::size_t column = 0; column < 4u; ++column)
    {
        ozz::math::StorePtrU(source.cols[column], &output[column][0]);
    }
    return output;
}

bool UnitInterval(float value)
{
    return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
}

bool RuntimeTransform(const Assets::AnimationTransform &source,
                      ozz::math::Transform &output)
{
    const float length_squared =
        source.rotation[0] * source.rotation[0] +
        source.rotation[1] * source.rotation[1] +
        source.rotation[2] * source.rotation[2] +
        source.rotation[3] * source.rotation[3];
    if (length_squared <= 1.0e-12f)
    {
        return false;
    }
    const float inverse_length = 1.0f / std::sqrt(length_squared);
    output.translation = ozz::math::Float3(
        source.translation[0], source.translation[1], source.translation[2]);
    output.rotation = ozz::math::Quaternion(
        source.rotation[0] * inverse_length,
        source.rotation[1] * inverse_length,
        source.rotation[2] * inverse_length,
        source.rotation[3] * inverse_length);
    output.scale =
        ozz::math::Float3(source.scale[0], source.scale[1], source.scale[2]);
    return true;
}

bool FillJoint(
    std::size_t index, const Assets::Skeleton &source,
    const std::vector<std::vector<std::size_t>> &children,
    ozz::animation::offline::RawSkeleton::Joint &output)
{
    const Assets::SkeletonBone *joint = source.Bone(
        static_cast<std::uint32_t>(index));
    if (joint == nullptr)
    {
        return false;
    }
    output.name = joint->name;
    if (!RuntimeTransform(joint->rest, output.transform))
    {
        return false;
    }
    output.children.resize(children[index].size());
    for (std::size_t child = 0; child < children[index].size(); ++child)
    {
        if (!FillJoint(children[index][child], source, children,
                       output.children[child]))
        {
            return false;
        }
    }
    return true;
}

bool BuildSkeleton(const Assets::Skeleton &source,
                   ozz::animation::Skeleton &output)
{
    if (source.BoneCount() == 0u)
    {
        return false;
    }
    std::vector<std::vector<std::size_t>> children(source.BoneCount());
    for (std::size_t index = 1; index < source.BoneCount(); ++index)
    {
        const Assets::SkeletonBone *joint =
            source.Bone(static_cast<std::uint32_t>(index));
        if (joint == nullptr || joint->parent < 0 ||
            static_cast<std::size_t>(joint->parent) >= index)
        {
            return false;
        }
        children[static_cast<std::size_t>(joint->parent)].push_back(index);
    }

    ozz::animation::offline::RawSkeleton raw;
    raw.roots.resize(1);
    if (!FillJoint(0u, source, children, raw.roots.front()) || !raw.Validate())
    {
        return false;
    }
    std::size_t visited = 0;
    bool order_matches = true;
    ozz::animation::offline::IterateJointsDF(
        raw, [&](const ozz::animation::offline::RawSkeleton::Joint &joint,
                 const ozz::animation::offline::RawSkeleton::Joint *) {
            order_matches =
                order_matches && visited < source.BoneCount() &&
                std::string_view(joint.name.data(), joint.name.size()) ==
                    source.BoneName(static_cast<std::uint32_t>(visited));
            ++visited;
        });
    if (!order_matches || visited != source.BoneCount())
    {
        return false;
    }
    const auto built = ozz::animation::offline::SkeletonBuilder{}(raw);
    if (!built)
    {
        return false;
    }
    output = std::move(*built);
    return true;
}

bool BuildAnimation(const Assets::Animation &source,
                    ozz::animation::Animation &output)
{
    ozz::animation::offline::RawAnimation raw;
    raw.name = source.Name();
    raw.duration = source.Duration();
    raw.tracks.resize(source.TrackCount());
    const auto tracks = source.Tracks();
    for (std::size_t track_index = 0; track_index < tracks.size();
         ++track_index)
    {
        const Assets::AnimationTrack &source_track = tracks[track_index];
        auto &target = raw.tracks[track_index];
        target.translations.reserve(source_track.samples.size());
        target.rotations.reserve(source_track.samples.size());
        target.scales.reserve(source_track.samples.size());
        for (const auto &sample : source_track.samples)
        {
            ozz::math::Transform transform;
            if (!RuntimeTransform(sample.value, transform))
            {
                return false;
            }
            target.translations.push_back({sample.time, transform.translation});
            target.rotations.push_back({sample.time, transform.rotation});
            target.scales.push_back({sample.time, transform.scale});
        }
    }
    if (!raw.Validate())
    {
        return false;
    }
    ozz::animation::offline::AnimationBuilder builder;
    builder.iframe_interval = 10.0f;
    const auto built = builder(raw);
    if (!built)
    {
        return false;
    }
    output = std::move(*built);
    return true;
}
} // namespace

struct AnimationBackend::Impl
{
    struct RuntimeSkeleton
    {
        std::shared_ptr<const Assets::Skeleton> source;
        ozz::animation::Skeleton skeleton;
    };

    struct RuntimeClip
    {
        std::shared_ptr<const Assets::Animation> source;
        ozz::animation::Animation animation;
    };

    struct Instance
    {
        explicit Instance(std::shared_ptr<RuntimeSkeleton> resource)
            : skeleton(std::move(resource))
        {
            contexts = {
                std::make_unique<ozz::animation::SamplingJob::Context>(
                    skeleton->skeleton.num_joints()),
                std::make_unique<ozz::animation::SamplingJob::Context>(
                    skeleton->skeleton.num_joints())};
            const std::size_t soa_count =
                static_cast<std::size_t>(skeleton->skeleton.num_soa_joints());
            source_locals.resize(soa_count);
            destination_locals.resize(soa_count);
            blended_locals.resize(soa_count);
            current_locals.resize(soa_count);
            std::copy(skeleton->skeleton.joint_rest_poses().begin(),
                      skeleton->skeleton.joint_rest_poses().end(),
                      source_locals.begin());
            destination_locals = source_locals;
            blended_locals = source_locals;
            current_locals = source_locals;
            model_joints.resize(skeleton->source->BoneCount(),
                                ozz::math::Float4x4::identity());
            palette.resize(skeleton->source->BoneCount(), glm::mat4(1.0f));
        }

        std::shared_ptr<RuntimeSkeleton> skeleton;
        std::array<std::shared_ptr<RuntimeClip>, 2> clips;
        std::array<std::unique_ptr<ozz::animation::SamplingJob::Context>, 2>
            contexts;
        std::vector<ozz::math::SoaTransform> source_locals;
        std::vector<ozz::math::SoaTransform> destination_locals;
        std::vector<ozz::math::SoaTransform> blended_locals;
        std::vector<ozz::math::SoaTransform> current_locals;
        std::vector<ozz::math::Float4x4> model_joints;
        std::vector<glm::mat4> palette;
    };

    Instance *Resolve(std::uint32_t slot)
    {
        return slot < instances.size() ? instances[slot].get() : nullptr;
    }

    const Instance *Resolve(std::uint32_t slot) const
    {
        return slot < instances.size() ? instances[slot].get() : nullptr;
    }

    std::shared_ptr<RuntimeSkeleton> GetSkeleton(
        std::shared_ptr<const Assets::Skeleton> source)
    {
        const auto existing = skeletons.find(source.get());
        if (existing != skeletons.end())
        {
            if (auto resource = existing->second.lock())
            {
                return resource;
            }
        }
        auto resource = std::make_shared<RuntimeSkeleton>();
        resource->source = std::move(source);
        if (!BuildSkeleton(*resource->source, resource->skeleton))
        {
            return {};
        }
        skeletons[resource->source.get()] = resource;
        return resource;
    }

    std::shared_ptr<RuntimeClip> GetClip(
        std::shared_ptr<const Assets::Animation> source)
    {
        const auto existing = animations.find(source.get());
        if (existing != animations.end())
        {
            if (auto resource = existing->second.lock())
            {
                return resource;
            }
        }
        auto resource = std::make_shared<RuntimeClip>();
        resource->source = std::move(source);
        if (!BuildAnimation(*resource->source, resource->animation))
        {
            return {};
        }
        animations[resource->source.get()] = resource;
        return resource;
    }

    bool EnsureClip(Instance &instance, std::size_t channel,
                    std::shared_ptr<const Assets::Animation> source)
    {
        if (instance.clips[channel] != nullptr &&
            instance.clips[channel]->source == source)
        {
            return true;
        }
        instance.clips[channel] = GetClip(std::move(source));
        if (instance.clips[channel] == nullptr)
        {
            return false;
        }
        instance.contexts[channel]->Invalidate();
        return true;
    }

    bool Sample(Instance &instance, std::size_t channel,
                std::shared_ptr<const Assets::Animation> clip, float ratio,
                std::vector<ozz::math::SoaTransform> &output)
    {
        if (!EnsureClip(instance, channel, std::move(clip)))
        {
            return false;
        }
        ozz::animation::SamplingJob sample;
        sample.animation = &instance.clips[channel]->animation;
        sample.context = instance.contexts[channel].get();
        sample.ratio = ratio;
        sample.output = ozz::make_span(output);
        return sample.Run();
    }

    std::vector<std::unique_ptr<Instance>> instances;
    std::unordered_map<const Assets::Skeleton *,
                       std::weak_ptr<RuntimeSkeleton>>
        skeletons;
    std::unordered_map<const Assets::Animation *, std::weak_ptr<RuntimeClip>>
        animations;
    std::uint32_t max_instances = 0;
    std::uint64_t failures = 0;
    bool initialized = false;
};

AnimationBackend::AnimationBackend() : impl_(std::make_unique<Impl>())
{
}

AnimationBackend::~AnimationBackend() = default;

bool AnimationBackend::Initialize(const AnimationSystemDesc &desc)
{
    Shutdown();
    if (desc.max_instances == 0u)
    {
        return false;
    }
    impl_->max_instances = desc.max_instances;
    impl_->instances.reserve(desc.max_instances);
    impl_->initialized = true;
    return true;
}

void AnimationBackend::Shutdown()
{
    impl_->instances.clear();
    impl_->animations.clear();
    impl_->skeletons.clear();
    impl_->max_instances = 0;
    impl_->failures = 0;
    impl_->initialized = false;
}

bool AnimationBackend::CreateInstance(
    std::uint32_t slot, std::shared_ptr<const Assets::Skeleton> skeleton)
{
    if (!impl_->initialized || skeleton == nullptr ||
        slot >= impl_->max_instances)
    {
        ++impl_->failures;
        return false;
    }
    if (slot >= impl_->instances.size())
    {
        impl_->instances.resize(static_cast<std::size_t>(slot) + 1u);
    }
    if (impl_->instances[slot] != nullptr)
    {
        ++impl_->failures;
        return false;
    }
    std::shared_ptr<Impl::RuntimeSkeleton> resource =
        impl_->GetSkeleton(std::move(skeleton));
    if (resource == nullptr)
    {
        ++impl_->failures;
        return false;
    }
    impl_->instances[slot] =
        std::make_unique<Impl::Instance>(std::move(resource));
    return true;
}

void AnimationBackend::DestroyInstance(std::uint32_t slot)
{
    if (slot < impl_->instances.size())
    {
        impl_->instances[slot].reset();
    }
}

bool AnimationBackend::FreezeSourcePose(std::uint32_t slot)
{
    Impl::Instance *instance = impl_->Resolve(slot);
    if (instance == nullptr)
    {
        ++impl_->failures;
        return false;
    }
    instance->source_locals = instance->current_locals;
    return true;
}

bool AnimationBackend::Evaluate(std::uint32_t slot,
                                const AnimationPoseRequest &request)
{
    Impl::Instance *instance = impl_->Resolve(slot);
    if (instance == nullptr || !UnitInterval(request.source_ratio) ||
        !UnitInterval(request.destination_ratio) ||
        !UnitInterval(request.destination_weight) ||
        (request.destination_clip == nullptr &&
         request.destination_weight != 0.0f))
    {
        ++impl_->failures;
        return false;
    }

    if (request.sample_source)
    {
        if (request.source_clip == nullptr)
        {
            const auto rest = instance->skeleton->skeleton.joint_rest_poses();
            std::copy(rest.begin(), rest.end(), instance->source_locals.begin());
        }
        else if (!impl_->Sample(*instance, 0u, request.source_clip,
                                request.source_ratio, instance->source_locals))
        {
            ++impl_->failures;
            return false;
        }
    }

    const std::vector<ozz::math::SoaTransform> *final_locals =
        &instance->source_locals;
    if (request.destination_clip != nullptr)
    {
        if (!impl_->Sample(*instance, 1u, request.destination_clip,
                           request.destination_ratio,
                           instance->destination_locals))
        {
            ++impl_->failures;
            return false;
        }
        std::array<ozz::animation::BlendingJob::Layer, 2> layers;
        layers[0].weight = 1.0f - request.destination_weight;
        layers[0].transform = ozz::make_span(instance->source_locals);
        layers[1].weight = request.destination_weight;
        layers[1].transform = ozz::make_span(instance->destination_locals);
        ozz::animation::BlendingJob blend;
        blend.layers = ozz::make_span(layers);
        blend.rest_pose = instance->skeleton->skeleton.joint_rest_poses();
        blend.output = ozz::make_span(instance->blended_locals);
        if (!blend.Run())
        {
            ++impl_->failures;
            return false;
        }
        final_locals = &instance->blended_locals;
    }

    instance->current_locals = *final_locals;
    ozz::animation::LocalToModelJob local_to_model;
    local_to_model.skeleton = &instance->skeleton->skeleton;
    local_to_model.input = ozz::make_span(instance->current_locals);
    local_to_model.output = ozz::make_span(instance->model_joints);
    if (!local_to_model.Run())
    {
        ++impl_->failures;
        return false;
    }
    for (std::uint32_t joint = 0;
         joint < instance->skeleton->source->BoneCount();
         ++joint)
    {
        instance->palette[joint] =
            Matrix(instance->model_joints[joint]) *
            *instance->skeleton->source->JointFromArmatureBind(joint);
    }
    return true;
}

std::span<const glm::mat4>
AnimationBackend::SkinningPalette(std::uint32_t slot) const
{
    const Impl::Instance *instance = impl_->Resolve(slot);
    return instance == nullptr
               ? std::span<const glm::mat4>{}
               : std::span<const glm::mat4>(instance->palette);
}

std::optional<glm::mat4>
AnimationBackend::ModelJoint(std::uint32_t slot, std::uint16_t joint) const
{
    const Impl::Instance *instance = impl_->Resolve(slot);
    if (instance == nullptr || joint >= instance->model_joints.size())
    {
        return std::nullopt;
    }
    return Matrix(instance->model_joints[joint]);
}

std::uint64_t AnimationBackend::RuntimeFailureCount() const
{
    return impl_->failures;
}

} // namespace CEngine::Animations::Ozz
