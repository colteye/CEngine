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
 * @brief Loads standard Ogg/Opus files without a CEngine envelope.
 * @author Erik Coltey
 */

#include "assets/audio_asset.h"

#include "assets/reader.h"
#include "logging/logger.h"

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
    if (!std::equal(Ogg.begin(), Ogg.end(), decoded.begin()))
    {
        Logging::Logger::Get().Error("assets", "audio file is not an Ogg/Opus stream");
        return false;
    }
    audio = std::move(decoded);
    return true;
}

} // namespace CEngine::Assets
