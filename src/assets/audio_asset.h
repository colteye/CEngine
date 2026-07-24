//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/audio_asset.h
 * @brief Loads standard WAV, FLAC, MP3, and Ogg/Vorbis files.
 * @author Erik Coltey
 */

#ifndef CENGINE_ASSETS_AUDIO_ASSET_H
#define CENGINE_ASSETS_AUDIO_ASSET_H

#include <cstddef>
#include <filesystem>
#include <vector>

namespace CEngine::Assets
{

using Audio = std::vector<std::byte>;

bool LoadAudioAsset(const std::filesystem::path &path, Audio &audio);

} // namespace CEngine::Assets

#endif
