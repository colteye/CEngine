//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/animation_asset.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "assets/animation_asset.h"

#include "log.h"

#include <cmath>
#include <unordered_map>

namespace CEngine::Assets
{
namespace
{
constexpr std::uint64_t FnvOffset = 14695981039346656037ull;
constexpr std::uint64_t FnvPrime = 1099511628211ull;
constexpr float QuaternionEpsilon = 1.0e-12f;
constexpr float UniformScaleTolerance = 1.0e-4f;
constexpr std::size_t MaxTotalSamples = 4u * 1024u * 1024u;

std::uint64_t StableId(std::string_view value)
{
    std::uint64_t hash = FnvOffset;
    for (const unsigned char byte : value)
    {
        hash ^= byte;
        hash *= FnvPrime;
    }
    return hash;
}

bool ValidTransform(const AnimationWire::AnimationTransform &transform)
{
    float quaternion_length = 0.0f;
    for (float component : transform.rotation)
    {
        if (!std::isfinite(component))
        {
            return false;
        }
        quaternion_length += component * component;
    }
    if (quaternion_length <= QuaternionEpsilon)
    {
        return false;
    }
    for (float component : transform.translation)
    {
        if (!std::isfinite(component))
        {
            return false;
        }
    }
    for (float component : transform.scale)
    {
        if (!std::isfinite(component) || component <= 0.0f)
        {
            return false;
        }
    }
    return std::abs(transform.scale[0] - transform.scale[1]) <=
               UniformScaleTolerance &&
           std::abs(transform.scale[0] - transform.scale[2]) <=
               UniformScaleTolerance;
}
} // namespace

Animation::Animation(Animation &&) noexcept = default;
Animation &Animation::operator=(Animation &&) noexcept = default;

bool LoadAnimationAsset(const std::filesystem::path &path, Animation &animation)
{
    AnimationWire::Animation decoded;
    if (!Decode(path, Type::Animation, decoded) ||
        decoded.skeleton.type != static_cast<std::uint32_t>(Type::Skeleton))
    {
        Logging::Logger::Get().Error("assets", "animation payload is invalid");
        return false;
    }

    std::size_t total_samples = 0;
    for (const AnimationTrack &track : decoded.tracks)
    {
        if (track.samples.size() > MaxTotalSamples - total_samples)
        {
            Logging::Logger::Get().Error(
                "assets", "animation sample budget is exceeded");
            return false;
        }
        total_samples += track.samples.size();
        float previous = -1.0f;
        for (const auto &sample : track.samples)
        {
            if (sample.time <= previous || sample.time > decoded.duration ||
                !ValidTransform(sample.value))
            {
                Logging::Logger::Get().Error(
                    "assets", "animation track data is invalid");
                return false;
            }
            previous = sample.time;
        }
    }

    std::vector<AnimationMarker> markers;
    markers.reserve(decoded.events.size());
    std::unordered_map<std::uint64_t, std::string_view> event_names;
    float previous = -1.0f;
    for (const auto &event : decoded.events)
    {
        const auto existing = event_names.find(event.id);
        if (event.time < previous || event.time > decoded.duration ||
            event.id != StableId(event.name) ||
            (existing != event_names.end() && existing->second != event.name))
        {
            Logging::Logger::Get().Error(
                "assets", "animation event metadata is invalid");
            return false;
        }
        event_names.emplace(event.id, event.name);
        previous = event.time;
        markers.push_back({event.time, event.id, event.name});
    }

    animation.name_ = std::move(decoded.name);
    animation.skeleton_ = {std::move(decoded.skeleton.path),
                           decoded.skeleton.guid,
                           static_cast<Type>(decoded.skeleton.type)};
    animation.duration_ = decoded.duration;
    animation.markers_ = std::move(markers);
    animation.tracks_ = std::move(decoded.tracks);
    return true;
}

std::uint32_t Animation::TrackCount() const
{
    return static_cast<std::uint32_t>(tracks_.size());
}

} // namespace CEngine::Assets
