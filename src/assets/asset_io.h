#ifndef CENGINE_ASSET_IO_H
#define CENGINE_ASSET_IO_H

#include "assets/asset_format.h"

#include <filesystem>
#include <vector>

namespace CEngine::Assets
{

class AssetFile
{
  public:
    bool Load(const std::filesystem::path &path);

    [[nodiscard]] AssetType Type() const
    {
        return static_cast<AssetType>(header_.asset_type);
    }
    [[nodiscard]] const Guid &GetGuid() const
    {
        return header_.guid;
    }
    [[nodiscard]] std::uint64_t SourceHash() const
    {
        return header_.source_hash;
    }
    [[nodiscard]] std::string_view PlatformTarget() const;

    [[nodiscard]] ByteView Payload() const;

  private:
    bool Validate();

    std::vector<std::uint8_t> bytes_;
    DiskAssetHeader header_;
};

bool LoadFileBytes(const std::filesystem::path &path, std::vector<std::uint8_t> &out_bytes);
} // namespace CEngine::Assets

#endif
