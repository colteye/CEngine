#ifndef CENGINE_ASSETS_CSCENE_READER_H
#define CENGINE_ASSETS_CSCENE_READER_H

#include "assets/asset_io.h"
#include "assets/cscene_format.h"

#include <filesystem>
#include <string>

namespace CEngine::Assets {

template <typename T> struct DiskView {
    const T* data = nullptr;
    std::size_t size = 0;
    const T* begin() const { return data; }
    const T* end() const { return data + size; }
    const T& operator[](std::size_t index) const { return data[index]; }
    bool Empty() const { return size == 0; }
};

class CSceneFile {
public:
    bool Load(const std::filesystem::path& path, std::string* error = nullptr);
    const CSceneFormat::DiskSceneSettings& Settings() const { return *settings_; }
    DiskView<CSceneFormat::DiskAssetReference> AssetReferences() const;
    DiskView<CSceneFormat::DiskSceneEntity> Entities() const;
    DiskView<CSceneFormat::DiskEntityClassBlock> ClassBlocks() const;
    DiskView<CSceneFormat::DiskEntityConnection> Connections() const;
    DiskView<std::uint32_t> ClassEntities(const CSceneFormat::DiskEntityClassBlock& block) const;
    ByteView ClassRecords(const CSceneFormat::DiskEntityClassBlock& block) const;
    ByteView ClassAuxiliary(const CSceneFormat::DiskEntityClassBlock& block) const;
    std::string_view String(std::uint32_t offset, std::uint32_t size) const;

private:
    bool Validate(std::string* error);
    const std::uint8_t* At(std::uint64_t offset) const { return payload_.data + offset; }

    AssetFile asset_;
    ByteView payload_;
    CSceneFormat::DiskSceneHeader header_ = {};
    const CSceneFormat::DiskSceneSettings* settings_ = nullptr;
};

} // namespace CEngine::Assets

#endif
