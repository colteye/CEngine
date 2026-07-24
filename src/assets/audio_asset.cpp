//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/audio_asset.cpp
 * @brief Loads standard WAV, FLAC, MP3, and Ogg/Vorbis files.
 * @author Erik Coltey
 */

#include "assets/audio_asset.h"

#include "assets/reader.h"
#include "log.h"

#include <array>

namespace CEngine::Assets
{

bool LoadAudioAsset(const std::filesystem::path &path, Audio &audio)
{
    Audio decoded;
    if (!ReadFile(path, decoded) || decoded.size() < 4u)
    {
        Logging::Logger::Get().Error("assets", "audio file is truncated");
        return false;
    }
    constexpr std::array<std::byte, 4> Ogg = {
        std::byte{'O'}, std::byte{'g'}, std::byte{'g'}, std::byte{'S'},
    };
    constexpr std::array<std::byte, 4> Flac = {
        std::byte{'f'}, std::byte{'L'}, std::byte{'a'}, std::byte{'C'},
    };
    constexpr std::array<std::byte, 4> Riff = {
        std::byte{'R'}, std::byte{'I'}, std::byte{'F'}, std::byte{'F'},
    };
    constexpr std::array<std::byte, 4> Wave = {
        std::byte{'W'}, std::byte{'A'}, std::byte{'V'}, std::byte{'E'},
    };
    constexpr std::array<std::byte, 3> Id3 = {
        std::byte{'I'}, std::byte{'D'}, std::byte{'3'},
    };
    const bool ogg = std::equal(Ogg.begin(), Ogg.end(), decoded.begin());
    const bool flac = std::equal(Flac.begin(), Flac.end(), decoded.begin());
    const bool wav = decoded.size() >= 12u && std::equal(Riff.begin(), Riff.end(), decoded.begin()) &&
                     std::equal(Wave.begin(), Wave.end(), decoded.begin() + 8);
    const bool mp3_id3 = std::equal(Id3.begin(), Id3.end(), decoded.begin());
    const bool mp3_frame = decoded[0] == std::byte{0xff} &&
                           (std::to_integer<unsigned int>(decoded[1]) & 0xe0u) == 0xe0u;
    if (!ogg && !flac && !wav && !mp3_id3 && !mp3_frame)
    {
        Logging::Logger::Get().Error("assets", "audio file is not WAV, FLAC, MP3, or Ogg/Vorbis");
        return false;
    }
    audio = std::move(decoded);
    return true;
}

} // namespace CEngine::Assets
