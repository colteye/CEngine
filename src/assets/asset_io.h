//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/asset_io.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_ASSET_IO_H
#define CENGINE_ASSET_IO_H

#include "assets/asset_format.h"

#include <filesystem>
#include <vector>

namespace CEngine::Assets
{

/**
 * @brief TODO: Describe AssetFile.
 */
class AssetFile
{
  public:
    /**
     * @brief TODO: Describe Load.
     *
     * @param path TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool Load(const std::filesystem::path &path);

    /**
     * @brief TODO: Describe Type.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] AssetType Type() const
    {
        return static_cast<AssetType>(header_.asset_type);
    }
    /**
     * @brief TODO: Describe GetGuid.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] const Guid &GetGuid() const
    {
        return header_.guid;
    }
    /**
     * @brief TODO: Describe SourceHash.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] std::uint64_t SourceHash() const
    {
        return header_.source_hash;
    }
    /**
     * @brief TODO: Describe PlatformTarget.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] std::string_view PlatformTarget() const;

    /**
     * @brief TODO: Describe Payload.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] ByteView Payload() const;

  private:
    /**
     * @brief TODO: Describe Validate.
     *
     * @return TODO: Describe the return value.
     */
    bool Validate();

    std::vector<std::uint8_t> bytes_;
    DiskAssetHeader header_;
};

/**
 * @brief TODO: Describe LoadFileBytes.
 *
 * @param path TODO: Describe this parameter.
 * @param out_bytes TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool LoadFileBytes(const std::filesystem::path &path, std::vector<std::uint8_t> &out_bytes);
} // namespace CEngine::Assets

#endif
