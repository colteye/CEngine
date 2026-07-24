//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/audio/audio_system.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "audio/audio_system.h"

#include "audio/audio_backend.h"
#include "audio/miniaudio/audio_backend.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace CEngine::Audio
{
namespace
{

bool Valid(const SpatialDesc &spatial)
{
    return spatial.min_distance >= 0.0f && spatial.max_distance >= spatial.min_distance &&
           std::isfinite(spatial.rolloff) && spatial.rolloff >= 0.0f &&
           std::isfinite(spatial.doppler_factor) && spatial.doppler_factor >= 0.0f &&
           std::isfinite(spatial.cone_inner_angle) && std::isfinite(spatial.cone_outer_angle) &&
           spatial.cone_inner_angle >= 0.0f && spatial.cone_outer_angle >= spatial.cone_inner_angle &&
           std::isfinite(spatial.cone_outer_gain) && spatial.cone_outer_gain >= 0.0f;
}

} // namespace

AudioSystem::AudioSystem(std::unique_ptr<IAudioBackend> backend) : backend_(std::move(backend))
{
}

AudioSystem::~AudioSystem()
{
    Shutdown();
}

bool AudioSystem::Initialize(const AudioSystemDesc &desc)
{
    Shutdown();
    if (desc.max_voices == 0 || desc.sample_rate < 8000 || desc.sample_rate > 192000)
    {
        return false;
    }
    if (backend_ == nullptr)
    {
        backend_ = std::make_unique<Miniaudio::AudioBackend>();
    }
    diagnostics_.voice_capacity = desc.max_voices;
    if (!backend_->Initialize(desc))
    {
        ++diagnostics_.backend_failures;
        diagnostics_.device_available = false;
        backend_.reset();
        initialized_ = !desc.required;
        return initialized_;
    }

    voices_.resize(desc.max_voices);
    free_voices_.reserve(desc.max_voices);
    for (std::uint32_t slot = desc.max_voices; slot > 0; --slot)
    {
        free_voices_.push_back(slot - 1);
    }
    diagnostics_.device_available = true;
    initialized_ = true;
    for (std::size_t index = 0; index < buses_.size(); ++index)
    {
        backend_->SetBus(static_cast<Bus>(index), buses_[index].gain, buses_[index].muted, 0.0f);
    }
    backend_->SetEnvironment(environment_);
    return true;
}

void AudioSystem::Shutdown()
{
    if (backend_ != nullptr)
    {
        backend_->Shutdown();
    }
    voices_.clear();
    free_voices_.clear();
    diagnostics_ = {};
    next_sequence_ = 1;
    initialized_ = false;
}

void AudioSystem::Update()
{
    if (!initialized_ || backend_ == nullptr)
    {
        return;
    }
    for (std::uint32_t slot = 0; slot < voices_.size(); ++slot)
    {
        VoiceSlot &voice = voices_[slot];
        if (voice.live && !voice.paused && !backend_->IsPlaying(slot))
        {
            Release(slot, true);
        }
    }
}

std::uint32_t AudioSystem::Acquire(int priority)
{
    if (!free_voices_.empty())
    {
        const std::uint32_t slot = free_voices_.back();
        free_voices_.pop_back();
        return slot;
    }

    std::uint32_t candidate = std::numeric_limits<std::uint32_t>::max();
    for (std::uint32_t slot = 0; slot < voices_.size(); ++slot)
    {
        const VoiceSlot &voice = voices_[slot];
        if (voice.paused || voice.stopping || voice.priority > priority)
        {
            continue;
        }
        if (candidate == std::numeric_limits<std::uint32_t>::max() ||
            voice.priority < voices_[candidate].priority ||
            (voice.priority == voices_[candidate].priority && voice.sequence < voices_[candidate].sequence))
        {
            candidate = slot;
        }
    }
    if (candidate == std::numeric_limits<std::uint32_t>::max())
    {
        ++diagnostics_.voices_rejected;
        return candidate;
    }
    backend_->DestroyVoice(candidate);
    VoiceSlot &voice = voices_[candidate];
    voice.generation = voice.generation == std::numeric_limits<std::uint32_t>::max() ? 1 : voice.generation + 1;
    voice = VoiceSlot{voice.generation};
    ++diagnostics_.voices_stolen;
    --diagnostics_.active_voices;
    return candidate;
}

VoiceHandle AudioSystem::Play(const PlayDesc &desc)
{
    if (!initialized_ || backend_ == nullptr || desc.audio == nullptr || desc.audio->empty() ||
        !std::isfinite(desc.gain) || desc.gain < 0.0f || !std::isfinite(desc.pitch) || desc.pitch <= 0.0f ||
        !std::isfinite(desc.start_seconds) || desc.start_seconds < 0.0f || !std::isfinite(desc.fade_in_seconds) ||
        desc.fade_in_seconds < 0.0f ||
        static_cast<std::size_t>(desc.bus) >= buses_.size() || !Valid(desc.spatial))
    {
        ++diagnostics_.voices_rejected;
        return {};
    }

    const std::uint32_t slot = Acquire(desc.priority);
    if (slot == std::numeric_limits<std::uint32_t>::max())
    {
        return {};
    }
    if (!backend_->CreateVoice(slot, desc))
    {
        ++diagnostics_.backend_failures;
        free_voices_.push_back(slot);
        return {};
    }

    VoiceSlot &voice = voices_[slot];
    voice.live = true;
    voice.priority = desc.priority;
    voice.sequence = next_sequence_++;
    voice.paused = false;
    voice.stopping = false;
    ++diagnostics_.active_voices;
    ++diagnostics_.voices_started;
    return VoiceHandle(slot, voice.generation);
}

bool AudioSystem::UpdateVoice(VoiceHandle handle, const VoiceUpdate &update)
{
    VoiceSlot *voice = Resolve(handle);
    if (voice == nullptr || backend_ == nullptr || !std::isfinite(update.gain) || update.gain < 0.0f ||
        !std::isfinite(update.pitch) || update.pitch <= 0.0f || !std::isfinite(update.occlusion) ||
        update.occlusion < 0.0f || update.occlusion > 1.0f || !std::isfinite(update.obstruction) ||
        update.obstruction < 0.0f ||
        update.obstruction > 1.0f || !Valid(update.spatial))
    {
        return false;
    }
    return backend_->UpdateVoice(handle.Index(), update);
}

bool AudioSystem::Stop(VoiceHandle handle, float fade_seconds)
{
    VoiceSlot *voice = Resolve(handle);
    if (voice == nullptr || backend_ == nullptr || !std::isfinite(fade_seconds) || fade_seconds < 0.0f)
    {
        return false;
    }
    if (!backend_->StopVoice(handle.Index(), fade_seconds))
    {
        return false;
    }
    voice->paused = false;
    voice->stopping = true;
    if (fade_seconds == 0.0f)
    {
        Release(handle.Index(), true);
    }
    return true;
}

bool AudioSystem::Pause(VoiceHandle handle)
{
    VoiceSlot *voice = Resolve(handle);
    if (voice == nullptr || backend_ == nullptr || voice->stopping || !backend_->SetPaused(handle.Index(), true))
    {
        return false;
    }
    voice->paused = true;
    return true;
}

bool AudioSystem::Resume(VoiceHandle handle)
{
    VoiceSlot *voice = Resolve(handle);
    if (voice == nullptr || backend_ == nullptr || voice->stopping || !voice->paused ||
        !backend_->SetPaused(handle.Index(), false))
    {
        return false;
    }
    voice->paused = false;
    return true;
}

bool AudioSystem::Seek(VoiceHandle handle, float seconds)
{
    return Resolve(handle) != nullptr && backend_ != nullptr && std::isfinite(seconds) && seconds >= 0.0f &&
           backend_->Seek(handle.Index(), seconds);
}

bool AudioSystem::IsPlaying(VoiceHandle handle) const
{
    const VoiceSlot *voice = Resolve(handle);
    return voice != nullptr && !voice->paused && backend_ != nullptr && backend_->IsPlaying(handle.Index());
}

float AudioSystem::CursorSeconds(VoiceHandle handle) const
{
    return Resolve(handle) != nullptr && backend_ != nullptr ? backend_->CursorSeconds(handle.Index()) : 0.0f;
}

void AudioSystem::SetListener(const Listener &listener)
{
    if (backend_ != nullptr)
    {
        backend_->SetListener(listener);
    }
}

void AudioSystem::SetBus(Bus bus, const BusState &state, float fade_seconds)
{
    const auto index = static_cast<std::size_t>(bus);
    if (index >= buses_.size() || !std::isfinite(state.gain) || state.gain < 0.0f ||
        !std::isfinite(fade_seconds) || fade_seconds < 0.0f)
    {
        return;
    }
    buses_[index] = state;
    if (backend_ != nullptr)
    {
        backend_->SetBus(bus, state.gain, state.muted, fade_seconds);
    }
}

BusState AudioSystem::GetBus(Bus bus) const
{
    const auto index = static_cast<std::size_t>(bus);
    return index < buses_.size() ? buses_[index] : BusState{};
}

void AudioSystem::SetEnvironment(const Environment &environment)
{
    if (!std::isfinite(environment.room_size) || !std::isfinite(environment.damping) ||
        !std::isfinite(environment.width) || !std::isfinite(environment.wet) || !std::isfinite(environment.dry))
    {
        return;
    }
    environment_.room_size = std::clamp(environment.room_size, 0.0f, 1.0f);
    environment_.damping = std::clamp(environment.damping, 0.0f, 1.0f);
    environment_.width = std::clamp(environment.width, 0.0f, 1.0f);
    environment_.wet = std::clamp(environment.wet, 0.0f, 1.0f);
    environment_.dry = std::clamp(environment.dry, 0.0f, 1.0f);
    if (backend_ != nullptr)
    {
        backend_->SetEnvironment(environment_);
    }
}

const Environment &AudioSystem::GetEnvironment() const
{
    return environment_;
}

Diagnostics AudioSystem::GetDiagnostics() const
{
    Diagnostics diagnostics = diagnostics_;
    if (backend_ != nullptr)
    {
        diagnostics.backend_failures += backend_->RuntimeFailureCount();
    }
    return diagnostics;
}

AudioSystem::VoiceSlot *AudioSystem::Resolve(VoiceHandle handle)
{
    if (!handle || handle.Index() >= voices_.size())
    {
        return nullptr;
    }
    VoiceSlot &voice = voices_[handle.Index()];
    return voice.live && voice.generation == handle.Generation() ? &voice : nullptr;
}

const AudioSystem::VoiceSlot *AudioSystem::Resolve(VoiceHandle handle) const
{
    if (!handle || handle.Index() >= voices_.size())
    {
        return nullptr;
    }
    const VoiceSlot &voice = voices_[handle.Index()];
    return voice.live && voice.generation == handle.Generation() ? &voice : nullptr;
}

void AudioSystem::Release(std::uint32_t slot, bool finished)
{
    if (slot >= voices_.size() || !voices_[slot].live)
    {
        return;
    }
    if (backend_ != nullptr)
    {
        backend_->DestroyVoice(slot);
    }
    VoiceSlot &voice = voices_[slot];
    const std::uint32_t next_generation =
        voice.generation == std::numeric_limits<std::uint32_t>::max() ? 1 : voice.generation + 1;
    voice = VoiceSlot{next_generation};
    free_voices_.push_back(slot);
    --diagnostics_.active_voices;
    if (finished)
    {
        ++diagnostics_.voices_finished;
    }
}

} // namespace CEngine::Audio
