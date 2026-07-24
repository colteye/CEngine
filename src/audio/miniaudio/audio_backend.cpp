//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/audio/miniaudio/audio_backend.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "audio/miniaudio/audio_backend.h"

#include "audio/audio_system.h"

#include <SDL3/SDL.h>
#include <extras/nodes/ma_reverb_node/ma_reverb_node.h>
#include <miniaudio.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace CEngine::Audio::Miniaudio
{
namespace
{

constexpr std::uint32_t OutputChannels = 2;
constexpr std::size_t MixFrames = 4096;

ma_attenuation_model MiniaudioAttenuation(AttenuationModel model)
{
    switch (model)
    {
    case AttenuationModel::None:
        return ma_attenuation_model_none;
    case AttenuationModel::Linear:
        return ma_attenuation_model_linear;
    case AttenuationModel::Exponential:
        return ma_attenuation_model_exponential;
    case AttenuationModel::Inverse:
    default:
        return ma_attenuation_model_inverse;
    }
}

float EffectiveGain(const VoiceUpdate &update)
{
    return update.gain * (1.0f - update.occlusion * 0.65f) * (1.0f - update.obstruction * 0.25f);
}

double OcclusionCutoff(const VoiceUpdate &update)
{
    const float blocked = std::clamp(update.occlusion * 0.8f + update.obstruction * 0.2f, 0.0f, 1.0f);
    return 20000.0 - static_cast<double>(blocked) * 18800.0;
}

} // namespace

struct AudioBackend::Impl
{
    struct DecodedClip
    {
        std::vector<float> samples;
        std::uint64_t frames = 0;
        std::uint32_t channels = 0;
        std::uint32_t sample_rate = 0;
    };

    struct Voice
    {
        std::shared_ptr<const Assets::Audio> asset;
        std::shared_ptr<DecodedClip> decoded;
        ma_decoder decoder{};
        ma_audio_buffer_ref buffer{};
        ma_sound sound{};
        ma_lpf_node low_pass{};
        bool decoder_initialized = false;
        bool buffer_initialized = false;
        bool sound_initialized = false;
        bool low_pass_initialized = false;
    };

    class StreamLock
    {
      public:
        explicit StreamLock(SDL_AudioStream *stream) : stream_(stream)
        {
            locked_ = stream_ != nullptr && SDL_LockAudioStream(stream_);
        }
        ~StreamLock()
        {
            if (locked_)
            {
                SDL_UnlockAudioStream(stream_);
            }
        }
        [[nodiscard]] bool Locked() const
        {
            return locked_;
        }

      private:
        SDL_AudioStream *stream_ = nullptr;
        bool locked_ = false;
    };

    static void SDLCALL FillAudio(void *user_data, SDL_AudioStream *stream, int additional_amount, int)
    {
        auto &self = *static_cast<Impl *>(user_data);
        constexpr int BytesPerFrame = static_cast<int>(sizeof(float) * OutputChannels);
        int frames_remaining = (std::max(additional_amount, 0) + BytesPerFrame - 1) / BytesPerFrame;
        while (frames_remaining > 0)
        {
            const std::uint32_t frames =
                static_cast<std::uint32_t>(std::min<int>(frames_remaining, static_cast<int>(MixFrames)));
            ma_uint64 frames_read = 0;
            const ma_result result = ma_engine_read_pcm_frames(&self.engine, self.mix_buffer.data(), frames, &frames_read);
            if (result != MA_SUCCESS || frames_read == 0)
            {
                self.callback_failures.fetch_add(1, std::memory_order_relaxed);
                break;
            }
            const int bytes = static_cast<int>(frames_read) * BytesPerFrame;
            if (!SDL_PutAudioStreamData(stream, self.mix_buffer.data(), bytes))
            {
                self.callback_failures.fetch_add(1, std::memory_order_relaxed);
                break;
            }
            frames_remaining -= static_cast<int>(frames_read);
        }
    }

    std::shared_ptr<DecodedClip> Decode(const std::shared_ptr<const Assets::Audio> &asset)
    {
        const auto found = decoded.find(asset.get());
        if (found != decoded.end())
        {
            if (std::shared_ptr<DecodedClip> clip = found->second.lock())
            {
                return clip;
            }
        }

        ma_decoder decoder{};
        const ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, sample_rate);
        if (ma_decoder_init_memory(asset->data(), asset->size(), &config, &decoder) != MA_SUCCESS)
        {
            return {};
        }

        ma_format format = ma_format_unknown;
        ma_uint32 channels = 0;
        ma_uint32 decoded_rate = 0;
        if (ma_decoder_get_data_format(&decoder, &format, &channels, &decoded_rate, nullptr, 0) != MA_SUCCESS ||
            format != ma_format_f32 || channels == 0 || channels > MA_MAX_CHANNELS || decoded_rate == 0)
        {
            ma_decoder_uninit(&decoder);
            return {};
        }

        auto clip = std::make_shared<DecodedClip>();
        clip->channels = channels;
        clip->sample_rate = decoded_rate;
        std::array<float, MixFrames * MA_MAX_CHANNELS> scratch{};
        for (;;)
        {
            ma_uint64 frames_read = 0;
            const ma_result result = ma_decoder_read_pcm_frames(&decoder, scratch.data(), MixFrames, &frames_read);
            if (result != MA_SUCCESS && result != MA_AT_END)
            {
                ma_decoder_uninit(&decoder);
                return {};
            }
            if (frames_read > 0)
            {
                const std::size_t samples = static_cast<std::size_t>(frames_read) * channels;
                clip->samples.insert(clip->samples.end(), scratch.begin(), scratch.begin() + samples);
                clip->frames += frames_read;
            }
            if (frames_read < MixFrames || result == MA_AT_END)
            {
                break;
            }
        }
        ma_decoder_uninit(&decoder);
        if (clip->frames == 0)
        {
            return {};
        }
        decoded[asset.get()] = clip;
        return clip;
    }

    ma_sound_group *Group(Bus bus)
    {
        switch (bus)
        {
        case Bus::Music:
            return &music;
        case Bus::Dialog:
            return &dialog;
        case Bus::SoundEffects:
            return &sound_effects;
        case Bus::Master:
        case Bus::Count:
        default:
            return &master;
        }
    }

    Voice *Find(std::uint32_t slot)
    {
        const auto found = voices.find(slot);
        return found != voices.end() ? found->second.get() : nullptr;
    }

    const Voice *Find(std::uint32_t slot) const
    {
        const auto found = voices.find(slot);
        return found != voices.end() ? found->second.get() : nullptr;
    }

    void Destroy(Voice &voice)
    {
        if (voice.sound_initialized)
        {
            ma_sound_uninit(&voice.sound);
            voice.sound_initialized = false;
        }
        if (voice.low_pass_initialized)
        {
            ma_lpf_node_uninit(&voice.low_pass, nullptr);
            voice.low_pass_initialized = false;
        }
        if (voice.buffer_initialized)
        {
            ma_audio_buffer_ref_uninit(&voice.buffer);
            voice.buffer_initialized = false;
        }
        if (voice.decoder_initialized)
        {
            ma_decoder_uninit(&voice.decoder);
            voice.decoder_initialized = false;
        }
    }

    ma_engine engine{};
    ma_sound_group master{};
    ma_sound_group music{};
    ma_sound_group sound_effects{};
    ma_sound_group dialog{};
    ma_reverb_node reverb{};
    SDL_AudioStream *stream = nullptr;
    std::unordered_map<std::uint32_t, std::unique_ptr<Voice>> voices;
    std::unordered_map<const Assets::Audio *, std::weak_ptr<DecodedClip>> decoded;
    std::array<float, MixFrames * OutputChannels> mix_buffer{};
    std::atomic<std::uint64_t> callback_failures{0};
    std::uint32_t sample_rate = 48000;
    bool owns_audio_subsystem = false;
    bool engine_initialized = false;
    bool reverb_initialized = false;
    bool master_initialized = false;
    bool music_initialized = false;
    bool sound_effects_initialized = false;
    bool dialog_initialized = false;
};

AudioBackend::AudioBackend() : impl_(std::make_unique<Impl>())
{
}

AudioBackend::~AudioBackend()
{
    Shutdown();
}

bool AudioBackend::Initialize(const AudioSystemDesc &desc)
{
    Shutdown();
    impl_->sample_rate = desc.sample_rate;

    ma_engine_config engine_config = ma_engine_config_init();
    engine_config.noDevice = MA_TRUE;
    engine_config.channels = OutputChannels;
    engine_config.sampleRate = desc.sample_rate;
    engine_config.listenerCount = 1;
    if (ma_engine_init(&engine_config, &impl_->engine) != MA_SUCCESS)
    {
        return false;
    }
    impl_->engine_initialized = true;

    ma_reverb_node_config reverb_config = ma_reverb_node_config_init(OutputChannels, desc.sample_rate);
    if (ma_reverb_node_init(ma_engine_get_node_graph(&impl_->engine), &reverb_config, nullptr, &impl_->reverb) !=
        MA_SUCCESS)
    {
        Shutdown();
        return false;
    }
    impl_->reverb_initialized = true;
    if (ma_sound_group_init(&impl_->engine, MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT, nullptr, &impl_->master) !=
        MA_SUCCESS)
    {
        Shutdown();
        return false;
    }
    impl_->master_initialized = true;
    if (ma_node_attach_output_bus(reinterpret_cast<ma_node *>(&impl_->master), 0,
                                  ma_node_graph_get_endpoint(ma_engine_get_node_graph(&impl_->engine)), 0) !=
        MA_SUCCESS ||
        ma_node_attach_output_bus(reinterpret_cast<ma_node *>(&impl_->reverb), 0,
                                  reinterpret_cast<ma_node *>(&impl_->master), 0) != MA_SUCCESS)
    {
        Shutdown();
        return false;
    }
    if (ma_sound_group_init(&impl_->engine, 0, &impl_->master, &impl_->music) != MA_SUCCESS)
    {
        Shutdown();
        return false;
    }
    impl_->music_initialized = true;
    if (ma_sound_group_init(&impl_->engine, MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT, nullptr,
                            &impl_->sound_effects) != MA_SUCCESS)
    {
        Shutdown();
        return false;
    }
    impl_->sound_effects_initialized = true;
    if (ma_node_attach_output_bus(reinterpret_cast<ma_node *>(&impl_->sound_effects), 0,
                                  reinterpret_cast<ma_node *>(&impl_->reverb), 0) != MA_SUCCESS)
    {
        Shutdown();
        return false;
    }
    if (ma_sound_group_init(&impl_->engine, 0, &impl_->master, &impl_->dialog) != MA_SUCCESS)
    {
        Shutdown();
        return false;
    }
    impl_->dialog_initialized = true;

    if (!SDL_WasInit(SDL_INIT_AUDIO))
    {
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
        {
            Shutdown();
            return false;
        }
        impl_->owns_audio_subsystem = true;
    }
    const SDL_AudioSpec spec{SDL_AUDIO_F32, static_cast<int>(OutputChannels), static_cast<int>(desc.sample_rate)};
    impl_->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &Impl::FillAudio, impl_.get());
    if (impl_->stream == nullptr || !SDL_ResumeAudioStreamDevice(impl_->stream))
    {
        std::cerr << "Failed to open the SDL3 audio device: " << SDL_GetError() << '\n';
        Shutdown();
        return false;
    }
    return true;
}

void AudioBackend::Shutdown()
{
    if (impl_ == nullptr)
    {
        return;
    }
    if (impl_->stream != nullptr)
    {
        SDL_DestroyAudioStream(impl_->stream);
        impl_->stream = nullptr;
    }
    for (auto &[slot, voice] : impl_->voices)
    {
        static_cast<void>(slot);
        impl_->Destroy(*voice);
    }
    impl_->voices.clear();
    impl_->decoded.clear();
    if (impl_->dialog_initialized)
    {
        ma_sound_group_uninit(&impl_->dialog);
        impl_->dialog_initialized = false;
    }
    if (impl_->sound_effects_initialized)
    {
        ma_sound_group_uninit(&impl_->sound_effects);
        impl_->sound_effects_initialized = false;
    }
    if (impl_->music_initialized)
    {
        ma_sound_group_uninit(&impl_->music);
        impl_->music_initialized = false;
    }
    if (impl_->master_initialized)
    {
        ma_sound_group_uninit(&impl_->master);
        impl_->master_initialized = false;
    }
    if (impl_->reverb_initialized)
    {
        ma_reverb_node_uninit(&impl_->reverb, nullptr);
        impl_->reverb_initialized = false;
    }
    if (impl_->engine_initialized)
    {
        ma_engine_uninit(&impl_->engine);
        impl_->engine_initialized = false;
    }
    if (impl_->owns_audio_subsystem)
    {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        impl_->owns_audio_subsystem = false;
    }
}

bool AudioBackend::CreateVoice(std::uint32_t slot, const PlayDesc &desc)
{
    if (impl_->stream == nullptr || impl_->Find(slot) != nullptr)
    {
        return false;
    }
    auto voice = std::make_unique<Impl::Voice>();
    voice->asset = desc.audio;
    ma_data_source *data_source = nullptr;
    if (desc.streaming)
    {
        const ma_decoder_config decoder_config = ma_decoder_config_init(ma_format_f32, 0, impl_->sample_rate);
        if (ma_decoder_init_memory(desc.audio->data(), desc.audio->size(), &decoder_config, &voice->decoder) !=
            MA_SUCCESS)
        {
            return false;
        }
        voice->decoder_initialized = true;
        data_source = reinterpret_cast<ma_data_source *>(&voice->decoder);
    }
    else
    {
        voice->decoded = impl_->Decode(desc.audio);
        if (voice->decoded == nullptr ||
            ma_audio_buffer_ref_init(ma_format_f32, voice->decoded->channels, voice->decoded->samples.data(),
                                     voice->decoded->frames, &voice->buffer) != MA_SUCCESS)
        {
            return false;
        }
        voice->buffer.sampleRate = voice->decoded->sample_rate;
        voice->buffer_initialized = true;
        data_source = reinterpret_cast<ma_data_source *>(&voice->buffer);
    }

    Impl::StreamLock lock(impl_->stream);
    if (!lock.Locked())
    {
        impl_->Destroy(*voice);
        return false;
    }
    if (ma_sound_init_from_data_source(&impl_->engine, data_source, MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT, nullptr,
                                       &voice->sound) != MA_SUCCESS)
    {
        impl_->Destroy(*voice);
        return false;
    }
    voice->sound_initialized = true;

    const ma_lpf_node_config lpf_config =
        ma_lpf_node_config_init(OutputChannels, impl_->sample_rate, 20000.0, 2);
    if (ma_lpf_node_init(ma_engine_get_node_graph(&impl_->engine), &lpf_config, nullptr, &voice->low_pass) !=
        MA_SUCCESS)
    {
        impl_->Destroy(*voice);
        return false;
    }
    voice->low_pass_initialized = true;
    if (ma_node_attach_output_bus(reinterpret_cast<ma_node *>(&voice->sound), 0,
                                  reinterpret_cast<ma_node *>(&voice->low_pass), 0) != MA_SUCCESS ||
        ma_node_attach_output_bus(reinterpret_cast<ma_node *>(&voice->low_pass), 0,
                                  reinterpret_cast<ma_node *>(impl_->Group(desc.bus)), 0) != MA_SUCCESS)
    {
        impl_->Destroy(*voice);
        return false;
    }

    VoiceUpdate update;
    update.spatial = desc.spatial;
    update.gain = desc.gain;
    update.pitch = desc.pitch;
    update.looping = desc.looping;
    ma_sound_set_spatialization_enabled(&voice->sound, update.spatial.enabled ? MA_TRUE : MA_FALSE);
    ma_sound_set_position(&voice->sound, update.spatial.position.x, update.spatial.position.y,
                          update.spatial.position.z);
    ma_sound_set_direction(&voice->sound, update.spatial.direction.x, update.spatial.direction.y,
                           update.spatial.direction.z);
    ma_sound_set_velocity(&voice->sound, update.spatial.velocity.x, update.spatial.velocity.y,
                          update.spatial.velocity.z);
    ma_sound_set_attenuation_model(&voice->sound, MiniaudioAttenuation(update.spatial.attenuation));
    ma_sound_set_min_distance(&voice->sound, update.spatial.min_distance);
    ma_sound_set_max_distance(&voice->sound, update.spatial.max_distance);
    ma_sound_set_rolloff(&voice->sound, update.spatial.rolloff);
    ma_sound_set_doppler_factor(&voice->sound, update.spatial.doppler_factor);
    ma_sound_set_cone(&voice->sound, update.spatial.cone_inner_angle, update.spatial.cone_outer_angle,
                      update.spatial.cone_outer_gain);
    ma_sound_set_volume(&voice->sound, update.gain);
    ma_sound_set_pitch(&voice->sound, update.pitch);
    ma_sound_set_looping(&voice->sound, update.looping ? MA_TRUE : MA_FALSE);
    if (desc.start_seconds > 0.0f && ma_sound_seek_to_second(&voice->sound, desc.start_seconds) != MA_SUCCESS)
    {
        impl_->Destroy(*voice);
        return false;
    }
    if (desc.fade_in_seconds > 0.0f)
    {
        ma_sound_set_fade_in_milliseconds(&voice->sound, 0.0f, desc.gain,
                                          static_cast<ma_uint64>(desc.fade_in_seconds * 1000.0f));
    }
    if (ma_sound_start(&voice->sound) != MA_SUCCESS)
    {
        impl_->Destroy(*voice);
        return false;
    }
    impl_->voices.emplace(slot, std::move(voice));
    return true;
}

void AudioBackend::DestroyVoice(std::uint32_t slot)
{
    auto found = impl_->voices.find(slot);
    if (found == impl_->voices.end())
    {
        return;
    }
    Impl::StreamLock lock(impl_->stream);
    impl_->Destroy(*found->second);
    impl_->voices.erase(found);
}

bool AudioBackend::UpdateVoice(std::uint32_t slot, const VoiceUpdate &update)
{
    Impl::Voice *voice = impl_->Find(slot);
    if (voice == nullptr)
    {
        return false;
    }
    Impl::StreamLock lock(impl_->stream);
    if (!lock.Locked())
    {
        return false;
    }
    ma_sound_set_spatialization_enabled(&voice->sound, update.spatial.enabled ? MA_TRUE : MA_FALSE);
    ma_sound_set_position(&voice->sound, update.spatial.position.x, update.spatial.position.y,
                          update.spatial.position.z);
    ma_sound_set_direction(&voice->sound, update.spatial.direction.x, update.spatial.direction.y,
                           update.spatial.direction.z);
    ma_sound_set_velocity(&voice->sound, update.spatial.velocity.x, update.spatial.velocity.y,
                          update.spatial.velocity.z);
    ma_sound_set_attenuation_model(&voice->sound, MiniaudioAttenuation(update.spatial.attenuation));
    ma_sound_set_min_distance(&voice->sound, update.spatial.min_distance);
    ma_sound_set_max_distance(&voice->sound, update.spatial.max_distance);
    ma_sound_set_rolloff(&voice->sound, update.spatial.rolloff);
    ma_sound_set_doppler_factor(&voice->sound, update.spatial.doppler_factor);
    ma_sound_set_cone(&voice->sound, update.spatial.cone_inner_angle, update.spatial.cone_outer_angle,
                      update.spatial.cone_outer_gain);
    ma_sound_set_volume(&voice->sound, EffectiveGain(update));
    ma_sound_set_pitch(&voice->sound, update.pitch);
    ma_sound_set_looping(&voice->sound, update.looping ? MA_TRUE : MA_FALSE);
    const ma_lpf_config lpf =
        ma_lpf_config_init(ma_format_f32, OutputChannels, impl_->sample_rate, OcclusionCutoff(update), 2);
    return ma_lpf_node_reinit(&lpf, &voice->low_pass) == MA_SUCCESS;
}

bool AudioBackend::StopVoice(std::uint32_t slot, float fade_seconds)
{
    Impl::Voice *voice = impl_->Find(slot);
    if (voice == nullptr)
    {
        return false;
    }
    Impl::StreamLock lock(impl_->stream);
    return (fade_seconds > 0.0f
                ? ma_sound_stop_with_fade_in_milliseconds(
                      &voice->sound, static_cast<ma_uint64>(fade_seconds * 1000.0f))
                : ma_sound_stop(&voice->sound)) == MA_SUCCESS;
}

bool AudioBackend::SetPaused(std::uint32_t slot, bool paused)
{
    Impl::Voice *voice = impl_->Find(slot);
    if (voice == nullptr)
    {
        return false;
    }
    Impl::StreamLock lock(impl_->stream);
    return (paused ? ma_sound_stop(&voice->sound) : ma_sound_start(&voice->sound)) == MA_SUCCESS;
}

bool AudioBackend::Seek(std::uint32_t slot, float seconds)
{
    Impl::Voice *voice = impl_->Find(slot);
    if (voice == nullptr)
    {
        return false;
    }
    Impl::StreamLock lock(impl_->stream);
    return ma_sound_seek_to_second(&voice->sound, seconds) == MA_SUCCESS;
}

bool AudioBackend::IsPlaying(std::uint32_t slot) const
{
    const Impl::Voice *voice = impl_->Find(slot);
    if (voice == nullptr)
    {
        return false;
    }
    Impl::StreamLock lock(impl_->stream);
    return ma_sound_is_playing(&voice->sound) == MA_TRUE;
}

float AudioBackend::CursorSeconds(std::uint32_t slot) const
{
    const Impl::Voice *voice = impl_->Find(slot);
    if (voice == nullptr)
    {
        return 0.0f;
    }
    Impl::StreamLock lock(impl_->stream);
    float seconds = 0.0f;
    return ma_sound_get_cursor_in_seconds(&voice->sound, &seconds) == MA_SUCCESS ? seconds : 0.0f;
}

void AudioBackend::SetListener(const Listener &listener)
{
    Impl::StreamLock lock(impl_->stream);
    if (!lock.Locked())
    {
        return;
    }
    ma_engine_listener_set_position(&impl_->engine, 0, listener.position.x, listener.position.y, listener.position.z);
    ma_engine_listener_set_direction(&impl_->engine, 0, listener.forward.x, listener.forward.y, listener.forward.z);
    ma_engine_listener_set_world_up(&impl_->engine, 0, listener.up.x, listener.up.y, listener.up.z);
    ma_engine_listener_set_velocity(&impl_->engine, 0, listener.velocity.x, listener.velocity.y, listener.velocity.z);
}

void AudioBackend::SetBus(Bus bus, float gain, bool muted, float fade_seconds)
{
    Impl::StreamLock lock(impl_->stream);
    if (!lock.Locked())
    {
        return;
    }
    ma_sound_group *group = impl_->Group(bus);
    const float target = muted ? 0.0f : gain;
    if (fade_seconds > 0.0f)
    {
        ma_sound_group_set_fade_in_milliseconds(group, -1.0f, target,
                                                static_cast<ma_uint64>(fade_seconds * 1000.0f));
    }
    else
    {
        ma_sound_group_set_volume(group, target);
    }
}

void AudioBackend::SetEnvironment(const Environment &environment)
{
    Impl::StreamLock lock(impl_->stream);
    if (!lock.Locked())
    {
        return;
    }
    verblib_set_room_size(&impl_->reverb.reverb, environment.room_size);
    verblib_set_damping(&impl_->reverb.reverb, environment.damping);
    verblib_set_width(&impl_->reverb.reverb, environment.width);
    verblib_set_wet(&impl_->reverb.reverb, environment.wet);
    verblib_set_dry(&impl_->reverb.reverb, environment.dry);
}

std::uint64_t AudioBackend::RuntimeFailureCount() const
{
    return impl_->callback_failures.load(std::memory_order_relaxed);
}

} // namespace CEngine::Audio::Miniaudio
