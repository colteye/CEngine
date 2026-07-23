#include "assets/cscene_reader.h"

#include <cstring>
#include <cmath>
#include <limits>

namespace CEngine::Assets {
namespace {
void SetError(std::string* error, const char* message) { if (error) *error = message; }

bool RangeFits(std::uint64_t offset, std::uint64_t count, std::uint64_t stride, std::size_t size)
{
    if (count != 0 && stride > std::numeric_limits<std::uint64_t>::max() / count) return false;
    const std::uint64_t bytes = count * stride;
    return offset <= size && bytes <= static_cast<std::uint64_t>(size) - offset;
}
} // namespace

bool CSceneFile::Load(const std::filesystem::path& path, std::string* error)
{
    payload_ = {};
    header_ = {};
    settings_ = nullptr;
    if (!asset_.Load(path, error)) return false;
    if (asset_.Type() != AssetType::Scene) { SetError(error, "asset is not a .cscene"); return false; }
    payload_ = asset_.Payload();
    return Validate(error);
}

bool CSceneFile::Validate(std::string* error)
{
    using namespace CSceneFormat;
    if (payload_.size < sizeof(header_)) { SetError(error, "scene payload is truncated"); return false; }
    std::memcpy(&header_, payload_.data, sizeof(header_));
    if (header_.magic != SceneMagic) { SetError(error, "scene magic is invalid"); return false; }
    if (header_.version != SceneContainerVersion || header_.header_size != sizeof(DiskSceneHeader))
    { SetError(error, "scene version or header size is unsupported"); return false; }
    if (!RangeFits(header_.settings_offset, 1, sizeof(DiskSceneSettings), payload_.size) ||
        !RangeFits(header_.asset_table_offset, header_.asset_count, header_.asset_stride, payload_.size) ||
        !RangeFits(header_.entity_table_offset, header_.entity_count, header_.entity_stride, payload_.size) ||
        !RangeFits(header_.class_table_offset, header_.class_count, header_.class_stride, payload_.size) ||
        !RangeFits(header_.connection_table_offset, header_.connection_count, header_.connection_stride, payload_.size) ||
        !RangeFits(header_.string_table_offset, 1, header_.string_table_size, payload_.size))
    { SetError(error, "scene table is outside the payload"); return false; }
    if ((header_.asset_count && header_.asset_stride != sizeof(DiskAssetReference)) ||
        (header_.entity_count && header_.entity_stride != sizeof(DiskSceneEntity)) ||
        (header_.class_count && header_.class_stride != sizeof(DiskEntityClassBlock)) ||
        (header_.connection_count && header_.connection_stride != sizeof(DiskEntityConnection)))
    { SetError(error, "scene table stride is unsupported"); return false; }
    settings_ = reinterpret_cast<const DiskSceneSettings*>(At(header_.settings_offset));
    if (!std::isfinite(settings_->ambient_color[0]) ||
        !std::isfinite(settings_->ambient_color[1]) ||
        !std::isfinite(settings_->ambient_color[2]) ||
        !std::isfinite(settings_->exposure) || settings_->exposure <= 0.0f ||
        !std::isfinite(settings_->gravity[0]) ||
        !std::isfinite(settings_->gravity[1]) ||
        !std::isfinite(settings_->gravity[2]))
    { SetError(error, "scene settings are invalid"); return false; }
    if (settings_->active_player_entity != InvalidEntityIndex &&
        settings_->active_player_entity >= header_.entity_count)
    { SetError(error, "scene active player index is invalid"); return false; }
    for (std::uint32_t value : settings_->reserved)
        if (value != 0) { SetError(error, "scene settings reserved fields must be zero"); return false; }
    for (const auto& asset : AssetReferences())
        if (asset.path_offset > header_.string_table_size || asset.path_size > header_.string_table_size - asset.path_offset)
        { SetError(error, "scene asset path is outside the string table"); return false; }
    for (const auto& entity : Entities())
    {
        if (entity.classname_offset > header_.string_table_size ||
            entity.classname_size > header_.string_table_size - entity.classname_offset ||
            entity.name_offset > header_.string_table_size || entity.name_size > header_.string_table_size - entity.name_offset)
        { SetError(error, "scene entity string is outside the string table"); return false; }
        const std::string_view classname = String(entity.classname_offset, entity.classname_size);
        if (classname.empty())
        { SetError(error, "scene entity classname must not be empty"); return false; }
    }
    for (const auto& block : ClassBlocks())
    {
        if (block.classname_offset > header_.string_table_size ||
            block.classname_size > header_.string_table_size - block.classname_offset ||
            block.classname_size == 0)
        { SetError(error, "entity class block classname is invalid"); return false; }
        if (block.count && block.record_stride == 0) { SetError(error, "entity class record stride is zero"); return false; }
        if (!RangeFits(block.entity_indices_offset, block.count, sizeof(std::uint32_t), payload_.size) ||
            !RangeFits(block.records_offset, block.count, block.record_stride, payload_.size) ||
            !RangeFits(block.auxiliary_offset, 1, block.auxiliary_size, payload_.size))
        { SetError(error, "entity class block is outside the payload"); return false; }
        const std::string_view block_classname = String(block.classname_offset, block.classname_size);
        for (std::uint32_t entity : ClassEntities(block))
        {
            if (entity >= header_.entity_count) { SetError(error, "entity class index is invalid"); return false; }
            const auto& row = Entities()[entity];
            if (String(row.classname_offset, row.classname_size) != block_classname)
            { SetError(error, "entity class block contains a different classname"); return false; }
        }
    }
    for (const auto& connection : Connections())
    {
        if (connection.source_entity >= header_.entity_count || connection.target_entity >= header_.entity_count)
        { SetError(error, "scene connection entity index is invalid"); return false; }
        if (connection.event_offset > header_.string_table_size ||
            connection.event_size > header_.string_table_size - connection.event_offset ||
            connection.action_offset > header_.string_table_size ||
            connection.action_size > header_.string_table_size - connection.action_offset ||
            connection.event_size == 0 || connection.action_size == 0)
        { SetError(error, "scene connection name is invalid"); return false; }
        if (!std::isfinite(connection.delay_seconds) || connection.delay_seconds < 0.0f)
        { SetError(error, "scene connection delay is invalid"); return false; }
    }
    return true;
}

DiskView<CSceneFormat::DiskAssetReference> CSceneFile::AssetReferences() const
{ return {reinterpret_cast<const CSceneFormat::DiskAssetReference*>(At(header_.asset_table_offset)), header_.asset_count}; }
DiskView<CSceneFormat::DiskSceneEntity> CSceneFile::Entities() const
{ return {reinterpret_cast<const CSceneFormat::DiskSceneEntity*>(At(header_.entity_table_offset)), header_.entity_count}; }
DiskView<CSceneFormat::DiskEntityClassBlock> CSceneFile::ClassBlocks() const
{ return {reinterpret_cast<const CSceneFormat::DiskEntityClassBlock*>(At(header_.class_table_offset)), header_.class_count}; }
DiskView<CSceneFormat::DiskEntityConnection> CSceneFile::Connections() const
{ return {reinterpret_cast<const CSceneFormat::DiskEntityConnection*>(At(header_.connection_table_offset)), header_.connection_count}; }
DiskView<std::uint32_t> CSceneFile::ClassEntities(const CSceneFormat::DiskEntityClassBlock& block) const
{ return {reinterpret_cast<const std::uint32_t*>(At(block.entity_indices_offset)), block.count}; }
ByteView CSceneFile::ClassRecords(const CSceneFormat::DiskEntityClassBlock& block) const
{ return {At(block.records_offset), static_cast<std::size_t>(block.count) * block.record_stride}; }
ByteView CSceneFile::ClassAuxiliary(const CSceneFormat::DiskEntityClassBlock& block) const
{ return {At(block.auxiliary_offset), static_cast<std::size_t>(block.auxiliary_size)}; }
std::string_view CSceneFile::String(std::uint32_t offset, std::uint32_t size) const
{ return {reinterpret_cast<const char*>(At(header_.string_table_offset + offset)), size}; }

} // namespace CEngine::Assets
