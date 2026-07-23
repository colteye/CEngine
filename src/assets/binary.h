//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/binary.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_ASSETS_BINARY_H
#define CENGINE_ASSETS_BINARY_H

#include "assets/asset_format.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace CEngine::Assets
{

/**
 * @brief TODO: Describe ReadU16LE.
 *
 * @param bytes TODO: Describe this parameter.
 * @param offset TODO: Describe this parameter.
 * @param value TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
inline bool ReadU16LE(ByteView bytes, std::size_t &offset, std::uint16_t &value)
{
    if (offset > bytes.size || bytes.size - offset < 2u)
    {
        return false;
    }
    value = static_cast<std::uint16_t>(bytes.data[offset]) |
            static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes.data[offset + 1u]) << 8u);
    offset += 2u;
    return true;
}

/**
 * @brief TODO: Describe ReadU32LE.
 *
 * @param bytes TODO: Describe this parameter.
 * @param offset TODO: Describe this parameter.
 * @param value TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
inline bool ReadU32LE(ByteView bytes, std::size_t &offset, std::uint32_t &value)
{
    if (offset > bytes.size || bytes.size - offset < 4u)
    {
        return false;
    }
    value = static_cast<std::uint32_t>(bytes.data[offset]) |
            (static_cast<std::uint32_t>(bytes.data[offset + 1u]) << 8u) |
            (static_cast<std::uint32_t>(bytes.data[offset + 2u]) << 16u) |
            (static_cast<std::uint32_t>(bytes.data[offset + 3u]) << 24u);
    offset += 4u;
    return true;
}

/**
 * @brief TODO: Describe ReadU64LE.
 *
 * @param bytes TODO: Describe this parameter.
 * @param offset TODO: Describe this parameter.
 * @param value TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
inline bool ReadU64LE(ByteView bytes, std::size_t &offset, std::uint64_t &value)
{
    if (offset > bytes.size || bytes.size - offset < 8u)
    {
        return false;
    }
    value = 0;
    for (std::uint32_t byte = 0; byte < 8u; ++byte)
    {
        value |= static_cast<std::uint64_t>(bytes.data[offset + byte]) << (byte * 8u);
    }
    offset += 8u;
    return true;
}

/**
 * @brief TODO: Describe ReadI32LE.
 *
 * @param bytes TODO: Describe this parameter.
 * @param offset TODO: Describe this parameter.
 * @param value TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
inline bool ReadI32LE(ByteView bytes, std::size_t &offset, std::int32_t &value)
{
    std::uint32_t bits = 0;
    if (!ReadU32LE(bytes, offset, bits))
    {
        return false;
    }
    std::memcpy(&value, &bits, sizeof(value));
    return true;
}

/**
 * @brief TODO: Describe ReadF32LE.
 *
 * @param bytes TODO: Describe this parameter.
 * @param offset TODO: Describe this parameter.
 * @param value TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
inline bool ReadF32LE(ByteView bytes, std::size_t &offset, float &value)
{
    std::uint32_t bits = 0;
    if (!ReadU32LE(bytes, offset, bits))
    {
        return false;
    }
    std::memcpy(&value, &bits, sizeof(value));
    return true;
}

} // namespace CEngine::Assets

#endif
