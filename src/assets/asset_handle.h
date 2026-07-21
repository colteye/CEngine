#ifndef CENGINE_ASSETS_ASSET_HANDLE_H
#define CENGINE_ASSETS_ASSET_HANDLE_H

#include <cstdint>
#include <limits>

namespace CEngine::Assets {

struct AssetHandle {
    static constexpr std::uint32_t InvalidIndex = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t index = InvalidIndex;
    std::uint32_t generation = 0;
    constexpr explicit operator bool() const { return index != InvalidIndex; }
};

constexpr bool operator==(AssetHandle left, AssetHandle right)
{ return left.index == right.index && left.generation == right.generation; }
constexpr bool operator!=(AssetHandle left, AssetHandle right) { return !(left == right); }

} // namespace CEngine::Assets

#endif
