//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/audio/miniaudio/audio_backend.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_AUDIO_MINIAUDIO_AUDIO_BACKEND_H
#define CENGINE_AUDIO_MINIAUDIO_AUDIO_BACKEND_H

#include "audio/audio_backend.h"

#include <memory>

namespace CEngine::Audio::Miniaudio
{

class AudioBackend final : public IAudioBackend
{
  public:
    AudioBackend();
    ~AudioBackend() override;

    bool Initialize(const AudioSystemDesc &desc) override;
    void Shutdown() override;
    bool CreateVoice(std::uint32_t slot, const PlayDesc &desc) override;
    void DestroyVoice(std::uint32_t slot) override;
    bool UpdateVoice(std::uint32_t slot, const VoiceUpdate &update) override;
    bool StopVoice(std::uint32_t slot, float fade_seconds) override;
    bool SetPaused(std::uint32_t slot, bool paused) override;
    bool Seek(std::uint32_t slot, float seconds) override;
    [[nodiscard]] bool IsPlaying(std::uint32_t slot) const override;
    [[nodiscard]] float CursorSeconds(std::uint32_t slot) const override;
    void SetListener(const Listener &listener) override;
    void SetBus(Bus bus, float gain, bool muted, float fade_seconds) override;
    void SetEnvironment(const Environment &environment) override;
    [[nodiscard]] std::uint64_t RuntimeFailureCount() const override;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace CEngine::Audio::Miniaudio

#endif
