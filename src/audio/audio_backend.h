// Copyright (c) CEngine contributors.

#ifndef CENGINE_AUDIO_AUDIO_BACKEND_H
#define CENGINE_AUDIO_AUDIO_BACKEND_H

#include <cstdint>

namespace CEngine::Audio
{

struct AudioSystemDesc;
struct Environment;
struct Listener;
struct PlayDesc;
struct VoiceUpdate;
enum class Bus : std::uint8_t;

class IAudioBackend
{
  public:
    IAudioBackend() = default;
    virtual ~IAudioBackend() = default;
    IAudioBackend(const IAudioBackend &) = delete;
    IAudioBackend &operator=(const IAudioBackend &) = delete;
    IAudioBackend(IAudioBackend &&) = delete;
    IAudioBackend &operator=(IAudioBackend &&) = delete;

    virtual bool Initialize(const AudioSystemDesc &desc) = 0;
    virtual void Shutdown() = 0;
    virtual bool CreateVoice(std::uint32_t slot, const PlayDesc &desc) = 0;
    virtual void DestroyVoice(std::uint32_t slot) = 0;
    virtual bool UpdateVoice(std::uint32_t slot, const VoiceUpdate &update) = 0;
    virtual bool StopVoice(std::uint32_t slot, float fade_seconds) = 0;
    virtual bool SetPaused(std::uint32_t slot, bool paused) = 0;
    virtual bool Seek(std::uint32_t slot, float seconds) = 0;
    [[nodiscard]] virtual bool IsPlaying(std::uint32_t slot) const = 0;
    [[nodiscard]] virtual float CursorSeconds(std::uint32_t slot) const = 0;
    virtual void SetListener(const Listener &listener) = 0;
    virtual void SetBus(Bus bus, float gain, bool muted, float fade_seconds) = 0;
    virtual void SetEnvironment(const Environment &environment) = 0;
    [[nodiscard]] virtual std::uint64_t RuntimeFailureCount() const = 0;
};

} // namespace CEngine::Audio

#endif
