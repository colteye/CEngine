#ifndef CENGINE_ASSET_IO_H
#define CENGINE_ASSET_IO_H

#include "assets/asset_format.h"

#include <filesystem>
#include <string>
#include <vector>

namespace CEngine::Assets {

class AssetFile {
public:
    bool Load(const std::filesystem::path& path, std::string* error = nullptr);

    AssetType Type() const { return static_cast<AssetType>(header.asset_type); }
    const Guid& GetGuid() const { return header.guid; }
    std::uint64_t SourceHash() const { return header.source_hash; }
    std::string_view PlatformTarget() const;

    ByteView Payload() const;

private:
    bool Validate(std::string* error);

    std::vector<std::uint8_t> bytes;
    DiskAssetHeader header;
};

bool LoadFileBytes(const std::filesystem::path& path, std::vector<std::uint8_t>& out_bytes,
    std::string* error = nullptr);
bool WriteBinaryAsset(const std::filesystem::path& path, const AssetWriteDesc& desc,
    std::string* error = nullptr);
bool HashFile(const std::filesystem::path& path, std::uint64_t& hash, std::string* error = nullptr);

} // namespace CEngine::Assets

#endif
