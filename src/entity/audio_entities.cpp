//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/entity/audio_entities.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "entity/audio_entities.h"

#include "assets/store.h"
#include "context.h"
#include "scene/scene.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

#include <glm/gtx/quaternion.hpp>

namespace CEngine::Entities
{
namespace
{

#ifdef CENGINE_ENABLE_AUDIO
Audio::Bus RuntimeBus(Generated::EngineEntities::AudioBus value)
{
    switch (value)
    {
    case Generated::EngineEntities::AudioBus::Music:
        return Audio::Bus::Music;
    case Generated::EngineEntities::AudioBus::SoundEffects:
        return Audio::Bus::SoundEffects;
    case Generated::EngineEntities::AudioBus::Dialog:
        return Audio::Bus::Dialog;
    }
    return Audio::Bus::SoundEffects;
}
#endif

Audio::AttenuationModel RuntimeAttenuation(Generated::EngineEntities::AudioAttenuation value)
{
    switch (value)
    {
    case Generated::EngineEntities::AudioAttenuation::None:
        return Audio::AttenuationModel::None;
    case Generated::EngineEntities::AudioAttenuation::Inverse:
        return Audio::AttenuationModel::Inverse;
    case Generated::EngineEntities::AudioAttenuation::Linear:
        return Audio::AttenuationModel::Linear;
    case Generated::EngineEntities::AudioAttenuation::Exponential:
        return Audio::AttenuationModel::Exponential;
    }
    return Audio::AttenuationModel::Inverse;
}

} // namespace

std::string_view AudioSourceEntity::Classname() const
{
    return "audio_source";
}

bool AudioSourceEntity::AcceptsInput(std::string_view input) const
{
    return input == "Play" || input == "Stop" || input == "Pause" || input == "Resume" ||
           Entity::AcceptsInput(input);
}

bool AudioSourceEntity::HasOutput(std::string_view output) const
{
    return output == "OnStarted" || output == "OnFinished" || output == "OnStopped" ||
           Entity::HasOutput(output);
}

bool AudioSourceEntity::HandleInput(Context &context, const Scene::EntityInput &input)
{
    if (input.name == "Play")
    {
        return Play(context, input.activator);
    }
    if (input.name == "Stop")
    {
        return Stop(context, input.activator, true);
    }
#ifdef CENGINE_ENABLE_AUDIO
    if (input.name == "Pause")
    {
        return context.audio != nullptr && voice_ && context.audio->Pause(voice_);
    }
    if (input.name == "Resume")
    {
        return context.audio != nullptr && voice_ && context.audio->Resume(voice_);
    }
#else
    if (input.name == "Pause" || input.name == "Resume")
    {
        return false;
    }
#endif
    return Entity::HandleInput(context, input);
}

void AudioSourceEntity::Initialize(Context &context)
{
    if (audio)
    {
        if (context.scene == nullptr || context.assets == nullptr)
        {
            throw std::runtime_error("audio source requires scene assets");
        }
        const Assets::Reference *reference = context.scene->AssetReference(audio.index);
        if (reference == nullptr || reference->type != Assets::Type::Audio)
        {
            throw std::runtime_error("audio source reference is invalid");
        }
        loaded_audio_ = context.assets->LoadAudio(*reference);
        if (!loaded_audio_)
        {
            throw std::runtime_error("could not load audio source");
        }
    }
    previous_position_ = GetTransform().position;
    position_valid_ = true;
    if (autoplay && Enabled())
    {
        Play(context, GetHandle());
    }
}

Audio::SpatialDesc AudioSourceEntity::Spatial() const
{
    Audio::SpatialDesc desc;
    desc.enabled = spatial;
    desc.position = GetTransform().position;
    desc.direction = glm::normalize(GetTransform().rotation * glm::vec3(0.0f, 0.0f, -1.0f));
    desc.attenuation = RuntimeAttenuation(attenuation);
    desc.min_distance = min_distance;
    desc.max_distance = max_distance;
    desc.rolloff = rolloff;
    desc.doppler_factor = doppler_factor;
    desc.cone_inner_angle = cone_inner_angle;
    desc.cone_outer_angle = cone_outer_angle;
    desc.cone_outer_gain = cone_outer_gain;
    return desc;
}

bool AudioSourceEntity::Play(Context &context, Scene::EntityHandle activator)
{
#ifdef CENGINE_ENABLE_AUDIO
    if (!Enabled() || context.audio == nullptr || !loaded_audio_)
    {
        return false;
    }
    if (voice_)
    {
        context.audio->Stop(voice_);
        voice_ = {};
    }
    Audio::PlayDesc desc;
    desc.audio = loaded_audio_;
    desc.bus = RuntimeBus(bus);
    desc.spatial = Spatial();
    desc.gain = gain;
    desc.pitch = pitch;
    desc.start_seconds = start_seconds;
    desc.fade_in_seconds = fade_in_seconds;
    desc.priority = static_cast<int>(std::min<std::uint32_t>(
        priority, static_cast<std::uint32_t>(std::numeric_limits<int>::max())));
    desc.looping = looping;
    desc.streaming = streaming;
    voice_ = context.audio->Play(desc);
    was_playing_ = static_cast<bool>(voice_);
    if (voice_ && context.scene != nullptr)
    {
        context.scene->Emit(GetHandle(), "OnStarted", activator);
    }
    return static_cast<bool>(voice_);
#else
    (void)context;
    (void)activator;
    return false;
#endif
}

bool AudioSourceEntity::Stop(Context &context, Scene::EntityHandle activator, bool emit)
{
#ifdef CENGINE_ENABLE_AUDIO
    if (context.audio == nullptr || !voice_)
    {
        return false;
    }
    const bool stopped = context.audio->Stop(voice_, fade_out_seconds);
    if (stopped && emit && context.scene != nullptr)
    {
        context.scene->Emit(GetHandle(), "OnStopped", activator);
    }
    if (stopped)
    {
        voice_ = {};
        was_playing_ = false;
    }
    return stopped;
#else
    (void)context;
    (void)activator;
    (void)emit;
    return false;
#endif
}

void AudioSourceEntity::Update(Context &context, float delta_seconds)
{
#ifdef CENGINE_ENABLE_AUDIO
    if (context.audio == nullptr || !voice_)
    {
        return;
    }
    const bool playing = context.audio->IsPlaying(voice_);
    if (!playing)
    {
        voice_ = {};
        if (was_playing_ && context.scene != nullptr)
        {
            context.scene->Emit(GetHandle(), "OnFinished", GetHandle());
        }
        was_playing_ = false;
        return;
    }
    Audio::VoiceUpdate update;
    update.spatial = Spatial();
    if (position_valid_ && delta_seconds > 0.0f)
    {
        update.spatial.velocity = (GetTransform().position - previous_position_) / delta_seconds;
    }
    update.gain = gain;
    update.pitch = pitch;
    update.looping = looping;
    context.audio->UpdateVoice(voice_, update);
    previous_position_ = GetTransform().position;
    position_valid_ = true;
#else
    (void)context;
    (void)delta_seconds;
#endif
}

void AudioSourceEntity::Shutdown(Context &context)
{
    Stop(context, {}, false);
    loaded_audio_.reset();
    position_valid_ = false;
    was_playing_ = false;
}

void AudioSourceEntity::OnEnabledChanged(Context &context, bool enabled)
{
    if (!enabled)
    {
        Stop(context, GetHandle(), false);
    }
}

std::string_view AudioEnvironmentEntity::Classname() const
{
    return "audio_environment";
}

void AudioEnvironmentEntity::Initialize(Context &context)
{
    Update(context, 0.0f);
}

void AudioEnvironmentEntity::Update(Context &context, float /*delta_seconds*/)
{
#ifdef CENGINE_ENABLE_AUDIO
    if (context.audio != nullptr)
    {
        context.audio->SetEnvironment({room_size, damping, width, wet, dry});
    }
#else
    (void)context;
#endif
}

void AudioEnvironmentEntity::Shutdown(Context &context)
{
#ifdef CENGINE_ENABLE_AUDIO
    if (context.audio != nullptr)
    {
        context.audio->SetEnvironment({});
    }
#else
    (void)context;
#endif
}

void AudioEnvironmentEntity::OnEnabledChanged(Context &context, bool enabled)
{
#ifdef CENGINE_ENABLE_AUDIO
    if (!enabled && context.audio != nullptr)
    {
        context.audio->SetEnvironment({});
        return;
    }
#else
    (void)enabled;
#endif
    Update(context, 0.0f);
}

} // namespace CEngine::Entities
