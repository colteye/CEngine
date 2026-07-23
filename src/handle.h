//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/handle.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_HANDLE_H
#define CENGINE_HANDLE_H

#include <cstdint>
#include <type_traits>

namespace CEngine
{

// Typed runtime identity owned by another subsystem. The tag prevents handle
// domains from being mixed. The low 32 bits select an entry and the high 32
// bits hold its generation, following the same entry-plus-serial model as
// Source entity handles. Append-only domains keep the generation fixed.
/**
 * @brief TODO: Describe Handle.
 *
 * @tparam Tag TODO: Describe this template parameter.
 */
template <typename Tag> class Handle
{
  public:
    /**
     * @brief TODO: Describe Handle.
     */
    constexpr Handle() = default;

    /**
     * @brief TODO: Describe Handle.
     *
     * @param index TODO: Describe this parameter.
     * @param generation TODO: Describe this parameter.
     */
    constexpr Handle(std::uint32_t index, std::uint32_t generation)
        : value_(generation == 0 ? 0 : (static_cast<std::uint64_t>(generation) << 32u) | index)
    {
    }

    /**
     * @brief TODO: Describe operatorbool.
     *
     * @return TODO: Describe the return value.
     */
    constexpr explicit operator bool() const
    {
        return value_ != 0;
    }

    /**
     * @brief TODO: Describe Index.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] constexpr std::uint32_t Index() const
    {
        return static_cast<std::uint32_t>(value_);
    }

    /**
     * @brief TODO: Describe Generation.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] constexpr std::uint32_t Generation() const
    {
        return static_cast<std::uint32_t>(value_ >> 32u);
    }

    /**
     * @brief TODO: Describe Value.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] constexpr std::uint64_t Value() const
    {
        return value_;
    }

    /**
     * @brief TODO: Describe operator==.
     *
     * @param left TODO: Describe this parameter.
     * @param right TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    friend constexpr bool operator==(Handle left, Handle right)
    {
        return left.value_ == right.value_;
    }

  private:
    std::uint64_t value_ = 0;
};

namespace Detail
{
struct HandleLayoutTag;
}
static_assert(sizeof(Handle<Detail::HandleLayoutTag>) == sizeof(std::uint64_t));
static_assert(std::is_trivially_copyable_v<Handle<Detail::HandleLayoutTag>>);

} // namespace CEngine

#endif
