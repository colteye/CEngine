//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/cscene_reader.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "assets/cscene_reader.h"

#include "assets/asset_error.h"
#include "assets/binary.h"

#include <cmath>
#include <cstring>
#include <limits>

namespace CEngine::Assets
{
namespace
{

/**
 * @brief TODO: Describe RangeFits.
 *
 * @param offset TODO: Describe this parameter.
 * @param count TODO: Describe this parameter.
 * @param stride TODO: Describe this parameter.
 * @param size TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool RangeFits(std::uint64_t offset, std::uint64_t count, std::uint64_t stride, std::size_t size)
{
    if (count != 0 && stride > std::numeric_limits<std::uint64_t>::max() / count)
    {
        return false;
    }
    const std::uint64_t bytes = count * stride;
    return offset <= size && bytes <= static_cast<std::uint64_t>(size) - offset;
}

/**
 * @brief TODO: Describe ReadHeader.
 *
 * @param bytes TODO: Describe this parameter.
 * @param value TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool ReadHeader(ByteView bytes, CSceneFormat::DiskSceneHeader &value)
{
    if (bytes.size < sizeof(value))
    {
        return false;
    }
    std::size_t offset = 0;
    std::memcpy(value.magic.data(), bytes.data, value.magic.size());
    offset += value.magic.size();
    return ReadU16LE(bytes, offset, value.version) && ReadU16LE(bytes, offset, value.header_size) &&
           ReadU64LE(bytes, offset, value.settings_offset) && ReadU64LE(bytes, offset, value.asset_table_offset) &&
           ReadU32LE(bytes, offset, value.asset_count) && ReadU32LE(bytes, offset, value.asset_stride) &&
           ReadU64LE(bytes, offset, value.entity_table_offset) && ReadU32LE(bytes, offset, value.entity_count) &&
           ReadU32LE(bytes, offset, value.entity_stride) && ReadU64LE(bytes, offset, value.class_table_offset) &&
           ReadU32LE(bytes, offset, value.class_count) && ReadU32LE(bytes, offset, value.class_stride) &&
           ReadU64LE(bytes, offset, value.connection_table_offset) &&
           ReadU32LE(bytes, offset, value.connection_count) && ReadU32LE(bytes, offset, value.connection_stride) &&
           ReadU64LE(bytes, offset, value.string_table_offset) && ReadU64LE(bytes, offset, value.string_table_size);
}

/**
 * @brief TODO: Describe ReadSettings.
 *
 * @param bytes TODO: Describe this parameter.
 * @param offset TODO: Describe this parameter.
 * @param value TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool ReadSettings(ByteView bytes, std::size_t offset, CSceneFormat::DiskSceneSettings &value)
{
    for (float &component : value.ambient_color)
    {
        if (!ReadF32LE(bytes, offset, component))
        {
            return false;
        }
    }
    if (!ReadF32LE(bytes, offset, value.exposure))
    {
        return false;
    }
    for (float &component : value.gravity)
    {
        if (!ReadF32LE(bytes, offset, component))
        {
            return false;
        }
    }
    if (!ReadU32LE(bytes, offset, value.active_entity))
    {
        return false;
    }
    for (std::uint32_t &reserved : value.reserved)
    {
        if (!ReadU32LE(bytes, offset, reserved))
        {
            return false;
        }
    }
    return true;
}

/**
 * @brief TODO: Describe ReadAssetReference.
 *
 * @param bytes TODO: Describe this parameter.
 * @param offset TODO: Describe this parameter.
 * @param value TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool ReadAssetReference(ByteView bytes, std::size_t offset, CSceneFormat::DiskAssetReference &value)
{
    if (offset > bytes.size || bytes.size - offset < value.guid.size())
    {
        return false;
    }
    std::memcpy(value.guid.data(), bytes.data + offset, value.guid.size());
    offset += value.guid.size();
    return ReadU32LE(bytes, offset, value.type) && ReadU32LE(bytes, offset, value.flags) &&
           ReadU32LE(bytes, offset, value.path_offset) && ReadU32LE(bytes, offset, value.path_size);
}

/**
 * @brief TODO: Describe ReadEntity.
 *
 * @param bytes TODO: Describe this parameter.
 * @param offset TODO: Describe this parameter.
 * @param value TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool ReadEntity(ByteView bytes, std::size_t offset, CSceneFormat::DiskSceneEntity &value)
{
    return ReadU32LE(bytes, offset, value.classname_offset) && ReadU32LE(bytes, offset, value.classname_size) &&
           ReadU32LE(bytes, offset, value.name_offset) && ReadU32LE(bytes, offset, value.name_size) &&
           ReadU32LE(bytes, offset, value.flags);
}

/**
 * @brief TODO: Describe ReadClassBlock.
 *
 * @param bytes TODO: Describe this parameter.
 * @param offset TODO: Describe this parameter.
 * @param value TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool ReadClassBlock(ByteView bytes, std::size_t offset, CSceneFormat::DiskEntityClassBlock &value)
{
    return ReadU32LE(bytes, offset, value.classname_offset) && ReadU32LE(bytes, offset, value.classname_size) &&
           ReadU16LE(bytes, offset, value.version) && ReadU16LE(bytes, offset, value.flags) &&
           ReadU32LE(bytes, offset, value.count) && ReadU32LE(bytes, offset, value.record_stride) &&
           ReadU64LE(bytes, offset, value.entity_indices_offset) && ReadU64LE(bytes, offset, value.records_offset) &&
           ReadU64LE(bytes, offset, value.auxiliary_offset) && ReadU64LE(bytes, offset, value.auxiliary_size);
}

/**
 * @brief TODO: Describe ReadConnection.
 *
 * @param bytes TODO: Describe this parameter.
 * @param offset TODO: Describe this parameter.
 * @param value TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool ReadConnection(ByteView bytes, std::size_t offset, CSceneFormat::DiskEntityConnection &value)
{
    return ReadU32LE(bytes, offset, value.source_entity) && ReadU32LE(bytes, offset, value.target_entity) &&
           ReadU32LE(bytes, offset, value.event_offset) && ReadU32LE(bytes, offset, value.event_size) &&
           ReadU32LE(bytes, offset, value.action_offset) && ReadU32LE(bytes, offset, value.action_size) &&
           ReadF32LE(bytes, offset, value.delay_seconds);
}

} // namespace

/**
 * @brief TODO: Describe CSceneFile::Load.
 *
 * @param path TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool CSceneFile::Load(const std::filesystem::path &path)
{
    payload_ = {};
    header_ = {};
    settings_ = {};
    asset_references_.clear();
    entities_.clear();
    class_blocks_.clear();
    connections_.clear();
    class_entities_.clear();
    if (!asset_.Load(path))
    {
        return false;
    }
    if (asset_.Type() != AssetType::Scene)
    {
        return AssetError("asset is not a .cscene");
    }
    payload_ = asset_.Payload();
    return Validate();
}

/**
 * @brief TODO: Describe CSceneFile::Validate.
 *
 * @return TODO: Describe the return value.
 */
bool CSceneFile::Validate()
{
    using namespace CSceneFormat;
    if (!ReadHeader(payload_, header_))
    {
        return AssetError("scene payload is truncated");
    }
    if (header_.magic != SceneMagic)
    {
        return AssetError("scene magic is invalid");
    }
    if (header_.version != SceneContainerVersion || header_.header_size != sizeof(DiskSceneHeader))
    {
        return AssetError("scene version or header size is unsupported");
    }
    if (!RangeFits(header_.settings_offset, 1, sizeof(DiskSceneSettings), payload_.size) ||
        !RangeFits(header_.asset_table_offset, header_.asset_count, header_.asset_stride, payload_.size) ||
        !RangeFits(header_.entity_table_offset, header_.entity_count, header_.entity_stride, payload_.size) ||
        !RangeFits(header_.class_table_offset, header_.class_count, header_.class_stride, payload_.size) ||
        !RangeFits(header_.connection_table_offset, header_.connection_count, header_.connection_stride,
                   payload_.size) ||
        !RangeFits(header_.string_table_offset, 1, header_.string_table_size, payload_.size))
    {
        return AssetError("scene table is outside the payload");
    }
    if ((header_.asset_count != 0 && header_.asset_stride != sizeof(DiskAssetReference)) ||
        (header_.entity_count != 0 && header_.entity_stride != sizeof(DiskSceneEntity)) ||
        (header_.class_count != 0 && header_.class_stride != sizeof(DiskEntityClassBlock)) ||
        (header_.connection_count != 0 && header_.connection_stride != sizeof(DiskEntityConnection)))
    {
        return AssetError("scene table stride is unsupported");
    }
    if (!ReadSettings(payload_, static_cast<std::size_t>(header_.settings_offset), settings_))
    {
        return AssetError("scene settings are truncated");
    }

    asset_references_.resize(header_.asset_count);
    for (std::uint32_t row = 0; row < header_.asset_count; ++row)
    {
        if (!ReadAssetReference(payload_,
                                static_cast<std::size_t>(header_.asset_table_offset) +
                                    (static_cast<std::size_t>(row) * header_.asset_stride),
                                asset_references_[row]))
        {
            return AssetError("scene asset table is truncated");
        }
    }

    entities_.resize(header_.entity_count);
    for (std::uint32_t row = 0; row < header_.entity_count; ++row)
    {
        if (!ReadEntity(payload_,
                        static_cast<std::size_t>(header_.entity_table_offset) +
                            (static_cast<std::size_t>(row) * header_.entity_stride),
                        entities_[row]))
        {
            return AssetError("scene entity table is truncated");
        }
    }

    class_blocks_.resize(header_.class_count);
    for (std::uint32_t row = 0; row < header_.class_count; ++row)
    {
        if (!ReadClassBlock(payload_,
                            static_cast<std::size_t>(header_.class_table_offset) +
                                (static_cast<std::size_t>(row) * header_.class_stride),
                            class_blocks_[row]))
        {
            return AssetError("scene class table is truncated");
        }
    }

    connections_.resize(header_.connection_count);
    for (std::uint32_t row = 0; row < header_.connection_count; ++row)
    {
        if (!ReadConnection(payload_,
                            static_cast<std::size_t>(header_.connection_table_offset) +
                                (static_cast<std::size_t>(row) * header_.connection_stride),
                            connections_[row]))
        {
            return AssetError("scene connection table is truncated");
        }
    }

    if (!std::isfinite(settings_.ambient_color[0]) || !std::isfinite(settings_.ambient_color[1]) ||
        !std::isfinite(settings_.ambient_color[2]) || !std::isfinite(settings_.exposure) ||
        settings_.exposure <= 0.0f || !std::isfinite(settings_.gravity[0]) || !std::isfinite(settings_.gravity[1]) ||
        !std::isfinite(settings_.gravity[2]))
    {
        return AssetError("scene settings are invalid");
    }
    if (settings_.active_entity != InvalidEntityIndex && settings_.active_entity >= header_.entity_count)
    {
        return AssetError("scene active entity index is invalid");
    }
    for (std::uint32_t value : settings_.reserved)
    {
        if (value != 0)
        {
            return AssetError("scene settings reserved fields must be zero");
        }
    }

    for (const auto &asset : asset_references_)
    {
        if (asset.path_offset > header_.string_table_size ||
            asset.path_size > header_.string_table_size - asset.path_offset)
        {
            return AssetError("scene asset path is outside the string table");
        }
    }

    for (const auto &entity : entities_)
    {
        if (entity.classname_offset > header_.string_table_size ||
            entity.classname_size > header_.string_table_size - entity.classname_offset ||
            entity.name_offset > header_.string_table_size ||
            entity.name_size > header_.string_table_size - entity.name_offset)
        {
            return AssetError("scene entity string is outside the string table");
        }
        if (String(entity.classname_offset, entity.classname_size).empty())
        {
            return AssetError("scene entity classname must not be empty");
        }
    }

    class_entities_.resize(class_blocks_.size());
    for (std::size_t block_index = 0; block_index < class_blocks_.size(); ++block_index)
    {
        const auto &block = class_blocks_[block_index];
        if (block.classname_offset > header_.string_table_size ||
            block.classname_size > header_.string_table_size - block.classname_offset || block.classname_size == 0)
        {
            return AssetError("entity class block classname is invalid");
        }
        if (block.count != 0 && block.record_stride == 0)
        {
            return AssetError("entity class record stride is zero");
        }
        if (!RangeFits(block.entity_indices_offset, block.count, sizeof(std::uint32_t), payload_.size) ||
            !RangeFits(block.records_offset, block.count, block.record_stride, payload_.size) ||
            !RangeFits(block.auxiliary_offset, 1, block.auxiliary_size, payload_.size))
        {
            return AssetError("entity class block is outside the payload");
        }
        auto &indices = class_entities_[block_index];
        indices.resize(block.count);
        auto offset = static_cast<std::size_t>(block.entity_indices_offset);
        for (std::uint32_t &entity : indices)
        {
            if (!ReadU32LE(payload_, offset, entity))
            {
                return AssetError("entity class index table is truncated");
            }
        }
        const std::string_view classname = String(block.classname_offset, block.classname_size);
        for (std::uint32_t entity : indices)
        {
            if (entity >= header_.entity_count)
            {
                return AssetError("entity class index is invalid");
            }
            const auto &row = entities_[entity];
            if (String(row.classname_offset, row.classname_size) != classname)
            {
                return AssetError("entity class block contains a different classname");
            }
        }
    }

    for (const auto &connection : connections_)
    {
        if (connection.source_entity >= header_.entity_count || connection.target_entity >= header_.entity_count)
        {
            return AssetError("scene connection entity index is invalid");
        }
        if (connection.event_offset > header_.string_table_size ||
            connection.event_size > header_.string_table_size - connection.event_offset ||
            connection.action_offset > header_.string_table_size ||
            connection.action_size > header_.string_table_size - connection.action_offset ||
            connection.event_size == 0 || connection.action_size == 0)
        {
            return AssetError("scene connection name is invalid");
        }
        if (!std::isfinite(connection.delay_seconds) || connection.delay_seconds < 0.0f)
        {
            return AssetError("scene connection delay is invalid");
        }
    }
    return true;
}

/**
 * @brief TODO: Describe CSceneFile::AssetReferences.
 *
 * @return TODO: Describe the return value.
 */
DiskView<CSceneFormat::DiskAssetReference> CSceneFile::AssetReferences() const
{
    return {asset_references_.data(), asset_references_.size()};
}

/**
 * @brief TODO: Describe CSceneFile::Entities.
 *
 * @return TODO: Describe the return value.
 */
DiskView<CSceneFormat::DiskSceneEntity> CSceneFile::Entities() const
{
    return {entities_.data(), entities_.size()};
}

/**
 * @brief TODO: Describe CSceneFile::ClassBlocks.
 *
 * @return TODO: Describe the return value.
 */
DiskView<CSceneFormat::DiskEntityClassBlock> CSceneFile::ClassBlocks() const
{
    return {class_blocks_.data(), class_blocks_.size()};
}

/**
 * @brief TODO: Describe CSceneFile::Connections.
 *
 * @return TODO: Describe the return value.
 */
DiskView<CSceneFormat::DiskEntityConnection> CSceneFile::Connections() const
{
    return {connections_.data(), connections_.size()};
}

/**
 * @brief TODO: Describe CSceneFile::ClassEntities.
 *
 * @param block TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
DiskView<std::uint32_t> CSceneFile::ClassEntities(const CSceneFormat::DiskEntityClassBlock &block) const
{
    for (std::size_t index = 0; index < class_blocks_.size(); ++index)
    {
        if (&class_blocks_[index] == &block)
        {
            return {class_entities_[index].data(), class_entities_[index].size()};
        }
    }
    return {};
}

/**
 * @brief TODO: Describe CSceneFile::ClassRecords.
 *
 * @param block TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
ByteView CSceneFile::ClassRecords(const CSceneFormat::DiskEntityClassBlock &block) const
{
    return {At(block.records_offset), static_cast<std::size_t>(block.count) * block.record_stride};
}

/**
 * @brief TODO: Describe CSceneFile::ClassAuxiliary.
 *
 * @param block TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
ByteView CSceneFile::ClassAuxiliary(const CSceneFormat::DiskEntityClassBlock &block) const
{
    return {At(block.auxiliary_offset), static_cast<std::size_t>(block.auxiliary_size)};
}

/**
 * @brief TODO: Describe CSceneFile::String.
 *
 * @param offset TODO: Describe this parameter.
 * @param size TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
std::string_view CSceneFile::String(std::uint32_t offset, std::uint32_t size) const
{
    return {reinterpret_cast<const char *>(At(header_.string_table_offset + offset)), size};
}

} // namespace CEngine::Assets
