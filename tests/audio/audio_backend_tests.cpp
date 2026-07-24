//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file tests/audio/audio_backend_tests.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "audio/audio_system.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

namespace
{

void AppendU16(CEngine::Assets::Audio &bytes, std::uint16_t value)
{
    bytes.push_back(static_cast<std::byte>(value & 0xffu));
    bytes.push_back(static_cast<std::byte>((value >> 8u) & 0xffu));
}

void AppendU32(CEngine::Assets::Audio &bytes, std::uint32_t value)
{
    AppendU16(bytes, static_cast<std::uint16_t>(value & 0xffffu));
    AppendU16(bytes, static_cast<std::uint16_t>(value >> 16u));
}

void AppendTag(CEngine::Assets::Audio &bytes, const char (&tag)[5])
{
    for (std::size_t index = 0; index < 4; ++index)
    {
        bytes.push_back(static_cast<std::byte>(tag[index]));
    }
}

std::shared_ptr<const CEngine::Assets::Audio> MakeWav()
{
    constexpr std::uint32_t SampleRate = 48000;
    constexpr std::uint32_t FrameCount = 4800;
    constexpr std::uint16_t Channels = 1;
    constexpr std::uint16_t BitsPerSample = 16;
    constexpr std::uint32_t DataBytes = FrameCount * Channels * (BitsPerSample / 8u);

    auto bytes = std::make_shared<CEngine::Assets::Audio>();
    bytes->reserve(44u + DataBytes);
    AppendTag(*bytes, "RIFF");
    AppendU32(*bytes, 36u + DataBytes);
    AppendTag(*bytes, "WAVE");
    AppendTag(*bytes, "fmt ");
    AppendU32(*bytes, 16u);
    AppendU16(*bytes, 1u);
    AppendU16(*bytes, Channels);
    AppendU32(*bytes, SampleRate);
    AppendU32(*bytes, SampleRate * Channels * (BitsPerSample / 8u));
    AppendU16(*bytes, Channels * (BitsPerSample / 8u));
    AppendU16(*bytes, BitsPerSample);
    AppendTag(*bytes, "data");
    AppendU32(*bytes, DataBytes);
    for (std::uint32_t frame = 0; frame < FrameCount; ++frame)
    {
        constexpr double Pi = 3.14159265358979323846;
        const double phase = 2.0 * Pi * 440.0 * static_cast<double>(frame) / SampleRate;
        const auto sample = static_cast<std::int16_t>(std::sin(phase) * 4096.0);
        AppendU16(*bytes, static_cast<std::uint16_t>(sample));
    }
    return bytes;
}

bool Expect(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
    }
    return condition;
}

} // namespace

int main()
{
    CEngine::Audio::AudioSystem audio;
    CEngine::Audio::AudioSystemDesc system_desc;
    system_desc.required = true;
    system_desc.max_voices = 8;
    if (!Expect(audio.Initialize(system_desc), "SDL3/miniaudio backend should initialize with the dummy device"))
    {
        return 1;
    }

    CEngine::Audio::Listener listener;
    listener.position = {0.0f, 1.0f, 5.0f};
    audio.SetListener(listener);

    CEngine::Audio::Environment environment;
    environment.room_size = 0.75f;
    environment.damping = 0.35f;
    environment.wet = 0.2f;
    audio.SetEnvironment(environment);
    audio.SetBus(CEngine::Audio::Bus::SoundEffects, {0.8f, false}, 0.01f);

    CEngine::Audio::PlayDesc play;
    play.audio = MakeWav();
    play.looping = true;
    play.streaming = true;
    play.spatial.enabled = true;
    play.spatial.position = {2.0f, 0.0f, 0.0f};
    const CEngine::Audio::VoiceHandle voice = audio.Play(play);
    if (!Expect(static_cast<bool>(voice), "valid WAV data should decode and create a voice"))
    {
        return 1;
    }

    CEngine::Audio::VoiceUpdate update;
    update.looping = true;
    update.spatial = play.spatial;
    update.spatial.position = {-2.0f, 0.0f, 0.0f};
    update.occlusion = 0.5f;
    update.obstruction = 0.25f;
    if (!Expect(audio.UpdateVoice(voice, update), "spatial and obstruction controls should update") ||
        !Expect(audio.Pause(voice) && audio.Resume(voice), "voice pause and resume should work") ||
        !Expect(audio.Seek(voice, 0.02f), "voice seek should work") ||
        !Expect(audio.Stop(voice, 0.0f), "voice stop should work"))
    {
        return 1;
    }

    const CEngine::Audio::Diagnostics diagnostics = audio.GetDiagnostics();
    return Expect(diagnostics.device_available && diagnostics.voices_started == 1 &&
                      diagnostics.active_voices == 0 && diagnostics.backend_failures == 0,
                  "backend diagnostics should remain healthy")
               ? 0
               : 1;
}
