#ifndef CENGINE_FOUNDATION_SLOT_HANDLE_H
#define CENGINE_FOUNDATION_SLOT_HANDLE_H

#include <cstdint>
#include <limits>

namespace CEngine
{

// Identity for a reusable slot owned by another runtime subsystem. The tag
// prevents handles from different owners being mixed while keeping the
// representation explicit and trivially copyable.
template <typename Tag> struct SlotHandle
{
    static constexpr std::uint32_t InvalidIndex = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t index = InvalidIndex;
    std::uint32_t generation = 0;

    constexpr explicit operator bool() const
    {
        return index != InvalidIndex;
    }

    friend constexpr bool operator==(SlotHandle left, SlotHandle right)
    {
        return left.index == right.index && left.generation == right.generation;
    }
};

} // namespace CEngine

#endif
