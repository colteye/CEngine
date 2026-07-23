//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/casset_loader.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_CASSET_LOADER_H
#define CENGINE_CASSET_LOADER_H

#include "assets/asset_io.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace CEngine::Assets
{

constexpr std::array<char, 4> CAssetPayloadMagic = {'C', 'E', 'C', 'A'};
constexpr std::uint16_t CAssetPayloadVersion = 2;

/**
 * @brief TODO: Describe CAssetCompositionType.
 */
enum class CAssetCompositionType : std::uint32_t
{
    Unknown = 0,
    Prefab = 1,
    Scene = 2,
};

/**
 * @brief TODO: Describe CAssetComponentKind.
 */
enum class CAssetComponentKind : std::uint32_t
{
    Unknown = 0,
    Mesh = 1,
    Material = 2,
    Skeleton = 3,
    Animation = 4,
};

#pragma pack(push, 1)
/**
 * @brief TODO: Describe DiskCAssetHeader.
 */
struct DiskCAssetHeader
{
    std::array<char, 4> magic = CAssetPayloadMagic;
    std::uint16_t version = CAssetPayloadVersion;
    std::uint16_t header_size = 0;
    std::uint32_t composition_type = 0;
    std::uint32_t object_count = 0;
    std::uint32_t object_table_offset = 0;
    std::uint32_t component_count = 0;
    std::uint32_t component_table_offset = 0;
    std::uint32_t string_table_offset = 0;
    std::uint32_t string_table_size = 0;
    std::uint32_t source_path_offset = 0;
    std::uint32_t source_path_size = 0;
    std::uint32_t collection_name_offset = 0;
    std::uint32_t collection_name_size = 0;
};

/**
 * @brief TODO: Describe DiskCAssetObject.
 */
struct DiskCAssetObject
{
    std::uint32_t name_offset = 0;
    std::uint32_t name_size = 0;
    std::uint32_t role = 0;
    std::uint32_t object_type = 0;
    std::uint32_t parent_name_offset = 0;
    std::uint32_t parent_name_size = 0;
    std::uint32_t first_component = 0;
    std::uint32_t component_count = 0;
    float world_from_local[16] = {};
};

/**
 * @brief TODO: Describe DiskCAssetComponent.
 */
struct DiskCAssetComponent
{
    std::uint32_t kind = 0;
    std::uint32_t path_offset = 0;
    std::uint32_t path_size = 0;
};
#pragma pack(pop)

static_assert(sizeof(DiskCAssetHeader) == 52, "DiskCAssetHeader must stay packed and stable.");
static_assert(sizeof(DiskCAssetObject) == 96, "DiskCAssetObject must stay packed and stable.");
static_assert(sizeof(DiskCAssetComponent) == 12, "DiskCAssetComponent must stay packed and stable.");

/**
 * @brief TODO: Describe CAssetObject.
 */
struct CAssetObject
{
    std::string_view name;
    std::uint32_t role = 0;
    std::uint32_t object_type = 0;
    std::string_view parent_name;
    std::uint32_t first_component = 0;
    std::uint32_t component_count = 0;
    std::array<float, 16> world_from_local = {};
};

constexpr std::uint32_t CAssetObjectRoleOccluder = 13;

/**
 * @brief TODO: Describe CAssetComponent.
 */
struct CAssetComponent
{
    CAssetComponentKind kind = CAssetComponentKind::Unknown;
    std::string_view path;
};

/**
 * @brief TODO: Describe CAsset.
 */
class CAsset
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
     * @brief TODO: Describe CompositionType.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] CAssetCompositionType CompositionType() const
    {
        return static_cast<CAssetCompositionType>(header_.composition_type);
    }
    /**
     * @brief TODO: Describe ObjectCount.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] std::uint32_t ObjectCount() const
    {
        return header_.object_count;
    }
    /**
     * @brief TODO: Describe ComponentCount.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] std::uint32_t ComponentCount() const
    {
        return header_.component_count;
    }
    /**
     * @brief TODO: Describe SourcePath.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] std::string_view SourcePath() const;
    /**
     * @brief TODO: Describe CollectionName.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] std::string_view CollectionName() const;

    /**
     * @brief TODO: Describe Object.
     *
     * @param index TODO: Describe this parameter.
     * @param object TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool Object(std::uint32_t index, CAssetObject &object) const;
    /**
     * @brief TODO: Describe Component.
     *
     * @param index TODO: Describe this parameter.
     * @param component TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool Component(std::uint32_t index, CAssetComponent &component) const;
    /**
     * @brief TODO: Describe Component.
     *
     * @param object TODO: Describe this parameter.
     * @param local_index TODO: Describe this parameter.
     * @param component TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool Component(const CAssetObject &object, std::uint32_t local_index, CAssetComponent &component) const;

  private:
    /**
     * @brief TODO: Describe Parse.
     *
     * @return TODO: Describe the return value.
     */
    bool Parse();
    /**
     * @brief TODO: Describe StringViewAt.
     *
     * @param offset TODO: Describe this parameter.
     * @param size TODO: Describe this parameter.
     * @param view TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool StringViewAt(std::uint32_t offset, std::uint32_t size, std::string_view &view) const;

    AssetFile file_;
    DiskCAssetHeader header_;
    ByteView object_table_;
    ByteView component_table_;
    ByteView string_table_;
};

} // namespace CEngine::Assets

#endif
