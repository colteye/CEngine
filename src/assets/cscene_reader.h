#ifndef CENGINE_ASSETS_CSCENE_READER_H
#define CENGINE_ASSETS_CSCENE_READER_H

#include "assets/asset_io.h"
#include "assets/cscene_format.h"

#include <filesystem>
#include <string>
#include <vector>

namespace CEngine::Assets
{

template <typename T> struct DiskView
{
    const T *data = nullptr;
    std::size_t size = 0;
    [[nodiscard]] const T *Begin() const
    {
        return data;
    }
    [[nodiscard]] const T *End() const
    {
        return size == 0 ? data : data + size;
    }
    const T &operator[](std::size_t index) const
    {
        return data[index];
    }
};

class CSceneFile
{
  public:
    bool Load(const std::filesystem::path &path);
    [[nodiscard]] const CSceneFormat::DiskSceneSettings &Settings() const
    {
        return settings_;
    }
    [[nodiscard]] DiskView<CSceneFormat::DiskAssetReference> AssetReferences() const;
    [[nodiscard]] DiskView<CSceneFormat::DiskSceneEntity> Entities() const;
    [[nodiscard]] DiskView<CSceneFormat::DiskEntityClassBlock> ClassBlocks() const;
    [[nodiscard]] DiskView<CSceneFormat::DiskEntityConnection> Connections() const;
    [[nodiscard]] DiskView<std::uint32_t> ClassEntities(const CSceneFormat::DiskEntityClassBlock &block) const;
    [[nodiscard]] ByteView ClassRecords(const CSceneFormat::DiskEntityClassBlock &block) const;
    [[nodiscard]] ByteView ClassAuxiliary(const CSceneFormat::DiskEntityClassBlock &block) const;
    [[nodiscard]] std::string_view String(std::uint32_t offset, std::uint32_t size) const;

  private:
    bool Validate();
    [[nodiscard]] const std::uint8_t *At(std::uint64_t offset) const
    {
        return payload_.data + offset;
    }

    AssetFile asset_;
    ByteView payload_;
    CSceneFormat::DiskSceneHeader header_ = {};
    CSceneFormat::DiskSceneSettings settings_ = {};
    std::vector<CSceneFormat::DiskAssetReference> asset_references_;
    std::vector<CSceneFormat::DiskSceneEntity> entities_;
    std::vector<CSceneFormat::DiskEntityClassBlock> class_blocks_;
    std::vector<CSceneFormat::DiskEntityConnection> connections_;
    std::vector<std::vector<std::uint32_t>> class_entities_;
};

} // namespace CEngine::Assets

#endif
