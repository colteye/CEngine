// Copyright (c) CEngine contributors.

#ifndef CENGINE_AUDIO_AUDIO_SYSTEM_H
#define CENGINE_AUDIO_AUDIO_SYSTEM_H

#include "assets/audio_asset.h"
#include "audio/audio_backend.h"
#include "handle.h"

#include <glm/vec3.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace CEngine::Audio
{

struct VoiceTag;
using VoiceHandle = Handle<VoiceTag>;

enum class Bus : std::uint8_t
{
    Master,
    Music,
    SoundEffects,
    Dialog,
    Count,
};

enum class AttenuationModel : std::uint8_t
{
    None,
    Inverse,
    Linear,
    Exponential,
};

struct AudioSystemDesc
{
    std::uint32_t max_voices = 128;
    std::uint32_t sample_rate = 48000;
    bool required = false;
};

struct Listener
{
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 forward = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 velocity = glm::vec3(0.0f);
};

struct SpatialDesc
{
    bool enabled = false;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 direction = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 velocity = glm::vec3(0.0f);
    AttenuationModel attenuation = AttenuationModel::Inverse;
    float min_distance = 1.0f;
    float max_distance = 100.0f;
    float rolloff = 1.0f;
    float doppler_factor = 1.0f;
    float cone_inner_angle = 6.28318530718f;
    float cone_outer_angle = 6.28318530718f;
    float cone_outer_gain = 1.0f;
};

struct PlayDesc
{
    std::shared_ptr<const Assets::Audio> audio;
    Bus bus = Bus::SoundEffects;
    SpatialDesc spatial;
    float gain = 1.0f;
    float pitch = 1.0f;
    float start_seconds = 0.0f;
    float fade_in_seconds = 0.0f;
    int priority = 0;
    bool looping = false;
    bool streaming = false;
};

struct VoiceUpdate
{
    SpatialDesc spatial;
    float gain = 1.0f;
    float pitch = 1.0f;
    float occlusion = 0.0f;
    float obstruction = 0.0f;
    bool looping = false;
};

struct Environment
{
    float room_size = 0.5f;
    float damping = 0.5f;
    float width = 1.0f;
    float wet = 0.0f;
    float dry = 1.0f;
};

struct BusState
{
    float gain = 1.0f;
    bool muted = false;
};

struct Diagnostics
{
    std::uint32_t active_voices = 0;
    std::uint32_t voice_capacity = 0;
    std::uint64_t voices_started = 0;
    std::uint64_t voices_finished = 0;
    std::uint64_t voices_stolen = 0;
    std::uint64_t voices_rejected = 0;
    std::uint64_t backend_failures = 0;
    bool device_available = false;
};

class AudioSystem
{
  public:
    explicit AudioSystem(std::unique_ptr<IAudioBackend> backend = {});
    ~AudioSystem();

    AudioSystem(const AudioSystem &) = delete;
    AudioSystem &operator=(const AudioSystem &) = delete;
    AudioSystem(AudioSystem &&) = delete;
    AudioSystem &operator=(AudioSystem &&) = delete;

    bool Initialize(const AudioSystemDesc &desc = {});
    void Shutdown();
    void Update();

    VoiceHandle Play(const PlayDesc &desc);
    bool UpdateVoice(VoiceHandle handle, const VoiceUpdate &update);
    bool Stop(VoiceHandle handle, float fade_seconds = 0.0f);
    bool Pause(VoiceHandle handle);
    bool Resume(VoiceHandle handle);
    bool Seek(VoiceHandle handle, float seconds);
    [[nodiscard]] bool IsPlaying(VoiceHandle handle) const;
    [[nodiscard]] float CursorSeconds(VoiceHandle handle) const;

    void SetListener(const Listener &listener);
    void SetBus(Bus bus, const BusState &state, float fade_seconds = 0.0f);
    [[nodiscard]] BusState GetBus(Bus bus) const;
    void SetEnvironment(const Environment &environment);
    [[nodiscard]] const Environment &GetEnvironment() const;
    [[nodiscard]] Diagnostics GetDiagnostics() const;

  private:
    struct VoiceSlot
    {
        std::uint32_t generation = 1;
        int priority = 0;
        std::uint64_t sequence = 0;
        bool live = false;
        bool paused = false;
        bool stopping = false;
    };

    [[nodiscard]] VoiceSlot *Resolve(VoiceHandle handle);
    [[nodiscard]] const VoiceSlot *Resolve(VoiceHandle handle) const;
    void Release(std::uint32_t slot, bool finished);
    [[nodiscard]] std::uint32_t Acquire(int priority);

    std::unique_ptr<IAudioBackend> backend_;
    std::vector<VoiceSlot> voices_;
    std::vector<std::uint32_t> free_voices_;
    std::array<BusState, static_cast<std::size_t>(Bus::Count)> buses_{};
    Environment environment_{};
    Diagnostics diagnostics_{};
    std::uint64_t next_sequence_ = 1;
    bool initialized_ = false;
};

} // namespace CEngine::Audio

#endif
