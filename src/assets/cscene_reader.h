//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/cscene_reader.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_ASSETS_CSCENE_READER_H
#define CENGINE_ASSETS_CSCENE_READER_H

#include "assets/asset_io.h"
#include "assets/cscene_format.h"

#include <filesystem>
#include <string>
#include <vector>

namespace CEngine::Assets
{

/**
 * @brief TODO: Describe DiskView.
 *
 * @tparam T TODO: Describe this template parameter.
 */
template <typename T> struct DiskView
{
    const T *data = nullptr;
    std::size_t size = 0;
    /**
     * @brief TODO: Describe Begin.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] const T *Begin() const
    {
        return data;
    }
    /**
     * @brief TODO: Describe End.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] const T *End() const
    {
        return size == 0 ? data : data + size;
    }
    /**
     * @brief TODO: Describe operator[].
     *
     * @param index TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    const T &operator[](std::size_t index) const
    {
        return data[index];
    }
};

/**
 * @brief TODO: Describe CSceneFile.
 */
class CSceneFile
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
     * @brief TODO: Describe Settings.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] const CSceneFormat::DiskSceneSettings &Settings() const
    {
        return settings_;
    }
    /**
     * @brief TODO: Describe AssetReferences.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] DiskView<CSceneFormat::DiskAssetReference> AssetReferences() const;
    /**
     * @brief TODO: Describe Entities.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] DiskView<CSceneFormat::DiskSceneEntity> Entities() const;
    /**
     * @brief TODO: Describe ClassBlocks.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] DiskView<CSceneFormat::DiskEntityClassBlock> ClassBlocks() const;
    /**
     * @brief TODO: Describe Connections.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] DiskView<CSceneFormat::DiskEntityConnection> Connections() const;
    /**
     * @brief TODO: Describe ClassEntities.
     *
     * @param block TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] DiskView<std::uint32_t> ClassEntities(const CSceneFormat::DiskEntityClassBlock &block) const;
    /**
     * @brief TODO: Describe ClassRecords.
     *
     * @param block TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] ByteView ClassRecords(const CSceneFormat::DiskEntityClassBlock &block) const;
    /**
     * @brief TODO: Describe ClassAuxiliary.
     *
     * @param block TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] ByteView ClassAuxiliary(const CSceneFormat::DiskEntityClassBlock &block) const;
    /**
     * @brief TODO: Describe String.
     *
     * @param offset TODO: Describe this parameter.
     * @param size TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] std::string_view String(std::uint32_t offset, std::uint32_t size) const;

  private:
    /**
     * @brief TODO: Describe Validate.
     *
     * @return TODO: Describe the return value.
     */
    bool Validate();
    /**
     * @brief TODO: Describe At.
     *
     * @param offset TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
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
