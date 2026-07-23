// Copyright (c) CEngine contributors.

#include "audio/audio_backend.h"
#include "audio/audio_system.h"

#include <glm/vec3.hpp>

#include <iostream>
#include <memory>
#include <unordered_map>

namespace
{

class FakeBackend final : public CEngine::Audio::IAudioBackend
{
  public:
    bool Initialize(const CEngine::Audio::AudioSystemDesc &) override
    {
        initialized = true;
        return true;
    }
    void Shutdown() override
    {
        voices.clear();
    }
    bool CreateVoice(std::uint32_t slot, const CEngine::Audio::PlayDesc &) override
    {
        voices[slot] = true;
        ++created;
        return true;
    }
    void DestroyVoice(std::uint32_t slot) override
    {
        voices.erase(slot);
        ++destroyed;
    }
    bool UpdateVoice(std::uint32_t slot, const CEngine::Audio::VoiceUpdate &) override
    {
        return voices.contains(slot);
    }
    bool StopVoice(std::uint32_t slot, float) override
    {
        if (!voices.contains(slot))
        {
            return false;
        }
        voices[slot] = false;
        return true;
    }
    bool SetPaused(std::uint32_t slot, bool paused) override
    {
        if (!voices.contains(slot))
        {
            return false;
        }
        voices[slot] = !paused;
        return true;
    }
    bool Seek(std::uint32_t slot, float seconds) override
    {
        cursor = seconds;
        return voices.contains(slot);
    }
    bool IsPlaying(std::uint32_t slot) const override
    {
        const auto found = voices.find(slot);
        return found != voices.end() && found->second;
    }
    float CursorSeconds(std::uint32_t) const override
    {
        return cursor;
    }
    void SetListener(const CEngine::Audio::Listener &value) override
    {
        listener = value;
    }
    void SetBus(CEngine::Audio::Bus bus, float gain, bool muted, float) override
    {
        last_bus = bus;
        last_bus_gain = muted ? 0.0f : gain;
    }
    void SetEnvironment(const CEngine::Audio::Environment &value) override
    {
        environment = value;
    }
    [[nodiscard]] std::uint64_t RuntimeFailureCount() const override
    {
        return runtime_failures;
    }

    std::unordered_map<std::uint32_t, bool> voices;
    CEngine::Audio::Listener listener;
    CEngine::Audio::Environment environment;
    CEngine::Audio::Bus last_bus = CEngine::Audio::Bus::Master;
    float last_bus_gain = 0.0f;
    float cursor = 0.0f;
    int created = 0;
    int destroyed = 0;
    std::uint64_t runtime_failures = 0;
    bool initialized = false;
};

bool Expect(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
    }
    return condition;
}

CEngine::Audio::PlayDesc Sound(int priority)
{
    auto bytes = std::make_shared<CEngine::Assets::Audio>();
    bytes->push_back(std::byte{1});
    CEngine::Audio::PlayDesc desc;
    desc.audio = std::move(bytes);
    desc.priority = priority;
    return desc;
}

bool VoiceLifetimeAndPolicy()
{
    auto backend = std::make_unique<FakeBackend>();
    FakeBackend *fake = backend.get();
    CEngine::Audio::AudioSystem audio(std::move(backend));
    CEngine::Audio::AudioSystemDesc system_desc;
    system_desc.max_voices = 2;
    if (!Expect(audio.Initialize(system_desc), "audio facade should initialize"))
    {
        return false;
    }

    const CEngine::Audio::VoiceHandle low = audio.Play(Sound(0));
    const CEngine::Audio::VoiceHandle high = audio.Play(Sound(10));
    const CEngine::Audio::VoiceHandle replacement = audio.Play(Sound(1));
    if (!Expect(low && high && replacement, "valid sounds should produce voice handles") ||
        !Expect(!audio.IsPlaying(low), "voice stealing must invalidate the old generation") ||
        !Expect(audio.IsPlaying(high) && audio.IsPlaying(replacement), "higher-priority voices should survive"))
    {
        return false;
    }

    if (!Expect(audio.Pause(replacement), "live voice should pause"))
    {
        return false;
    }
    fake->voices[replacement.Index()] = false;
    audio.Update();
    if (!Expect(audio.Resume(replacement), "paused voice must not be collected as finished") ||
        !Expect(audio.Seek(replacement, 0.25f) && audio.CursorSeconds(replacement) == 0.25f,
                "voice seek/cursor should pass through"))
    {
        return false;
    }
    if (!Expect(audio.Stop(replacement), "voice should stop") ||
        !Expect(!audio.Stop(replacement), "stale stopped handle should be rejected"))
    {
        return false;
    }

    const CEngine::Audio::Diagnostics diagnostics = audio.GetDiagnostics();
    return Expect(diagnostics.voices_started == 3 && diagnostics.voices_stolen == 1,
                  "diagnostics should report starts and steals") &&
           Expect(fake->created == 3 && fake->destroyed >= 2, "backend lifetime should be symmetric");
}

bool ParametersPassThrough()
{
    auto backend = std::make_unique<FakeBackend>();
    FakeBackend *fake = backend.get();
    CEngine::Audio::AudioSystem audio(std::move(backend));
    if (!audio.Initialize())
    {
        return false;
    }

    CEngine::Audio::Listener listener;
    listener.position = glm::vec3(1.0f, 2.0f, 3.0f);
    audio.SetListener(listener);
    CEngine::Audio::Environment environment;
    environment.room_size = 0.8f;
    environment.wet = 0.4f;
    audio.SetEnvironment(environment);
    audio.SetBus(CEngine::Audio::Bus::Music, {0.25f, false}, 0.1f);
    fake->runtime_failures = 2;

    return Expect(fake->listener.position == listener.position, "listener should reach backend") &&
           Expect(fake->environment.room_size == 0.8f && fake->environment.wet == 0.4f,
                  "environment should reach backend") &&
           Expect(fake->last_bus == CEngine::Audio::Bus::Music && fake->last_bus_gain == 0.25f,
                  "bus settings should reach backend") &&
           Expect(audio.GetDiagnostics().backend_failures == 2, "runtime backend failures should reach diagnostics");
}

} // namespace

int main()
{
    return VoiceLifetimeAndPolicy() && ParametersPassThrough() ? 0 : 1;
}
