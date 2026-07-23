#ifndef CENGINE_INPUT_ACTION_H
#define CENGINE_INPUT_ACTION_H

#include <cstdint>
#include <limits>

namespace CEngine::Input
{

struct ActionId
{
    static constexpr std::uint32_t Invalid = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t index = Invalid;
    constexpr explicit operator bool() const
    {
        return index != Invalid;
    }
};

enum class PointerAxis
{
    X,
    Y,
};

} // namespace CEngine::Input

#endif
