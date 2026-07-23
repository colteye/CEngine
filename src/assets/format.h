//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/format.h
 * @brief Common version-one cooked asset envelope.
 * @author Erik Coltey
 */

#ifndef CENGINE_ASSETS_FORMAT_H
#define CENGINE_ASSETS_FORMAT_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace CEngine::Assets
{

using Guid = std::array<std::uint8_t, 16>;

inline constexpr std::array<std::byte, 4> Magic = {
    std::byte{'C'},
    std::byte{'E'},
    std::byte{'A'},
    std::byte{'F'},
};
inline constexpr std::uint16_t Version = 1;
inline constexpr std::uint16_t HeaderSize = 76;
inline constexpr std::size_t PlatformSize = 16;

enum class Type : std::uint32_t
{
    Unknown = 0,
    Texture,
    Material,
    Mesh,
    Skeleton,
    Animation,
    Physics,
    Prefab,
    Scene,
    Audio,
    Vfx,
    Navigation,
    Shader,
    Package,
    Asset,
    Particle,
};

struct Header
{
    Type type = Type::Unknown;
    Guid guid = {};
    std::uint64_t source_hash = 0;
    std::string platform;
};

struct Reference
{
    std::string path;
    Guid guid = {};
    Type type = Type::Unknown;
};

} // namespace CEngine::Assets

#endif
