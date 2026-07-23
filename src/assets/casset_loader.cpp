#include "assets/casset_loader.h"

#include "assets/asset_error.h"
#include "assets/binary.h"

#include <cstring>

namespace CEngine::Assets {
namespace {

bool ReadHeader(ByteView bytes, DiskCAssetHeader& value)
{
    if (bytes.size < sizeof(value)) return false;
    std::size_t offset = 0;
    std::memcpy(value.magic.data(), bytes.data, value.magic.size());
    offset += value.magic.size();
    return ReadU16LE(bytes, offset, value.version) &&
        ReadU16LE(bytes, offset, value.header_size) &&
        ReadU32LE(bytes, offset, value.composition_type) &&
        ReadU32LE(bytes, offset, value.object_count) &&
        ReadU32LE(bytes, offset, value.object_table_offset) &&
        ReadU32LE(bytes, offset, value.component_count) &&
        ReadU32LE(bytes, offset, value.component_table_offset) &&
        ReadU32LE(bytes, offset, value.string_table_offset) &&
        ReadU32LE(bytes, offset, value.string_table_size) &&
        ReadU32LE(bytes, offset, value.source_path_offset) &&
        ReadU32LE(bytes, offset, value.source_path_size) &&
        ReadU32LE(bytes, offset, value.collection_name_offset) &&
        ReadU32LE(bytes, offset, value.collection_name_size);
}

bool ReadObjectRow(
    ByteView bytes, std::size_t offset, DiskCAssetObject& value)
{
    if (!ReadU32LE(bytes, offset, value.name_offset) ||
        !ReadU32LE(bytes, offset, value.name_size) ||
        !ReadU32LE(bytes, offset, value.role) ||
        !ReadU32LE(bytes, offset, value.object_type) ||
        !ReadU32LE(bytes, offset, value.parent_name_offset) ||
        !ReadU32LE(bytes, offset, value.parent_name_size) ||
        !ReadU32LE(bytes, offset, value.first_component) ||
        !ReadU32LE(bytes, offset, value.component_count))
        return false;
    for (float& component : value.world_from_local)
        if (!ReadF32LE(bytes, offset, component)) return false;
    return true;
}

bool ReadComponentRow(
    ByteView bytes, std::size_t offset, DiskCAssetComponent& value)
{
    return ReadU32LE(bytes, offset, value.kind) &&
        ReadU32LE(bytes, offset, value.path_offset) &&
        ReadU32LE(bytes, offset, value.path_size);
}

} // namespace

bool CAsset::Load(const std::filesystem::path& path)
{
    file = {};
    header = {};
    object_table = {};
    component_table = {};
    string_table = {};

    if (!file.Load(path))
    {
        return false;
    }
    return Parse();
}

bool CAsset::Parse()
{
    if (file.Type() != AssetType::Asset)
    {
        return AssetError("asset is not a .casset composition");
    }

    const ByteView payload = file.Payload();
    if (payload.size < sizeof(DiskCAssetHeader))
    {
        return AssetError("casset payload is invalid");
    }

    if (!ReadHeader(payload, header))
        return AssetError("casset payload is invalid");
    if (header.magic != CAssetPayloadMagic ||
        header.version != CAssetPayloadVersion ||
        header.header_size != sizeof(DiskCAssetHeader))
    {
        return AssetError("casset payload header is not supported");
    }

    const std::size_t object_table_size =
        static_cast<std::size_t>(header.object_count) * sizeof(DiskCAssetObject);
    const std::size_t component_table_size =
        static_cast<std::size_t>(header.component_count) * sizeof(DiskCAssetComponent);
    if (header.object_table_offset > payload.size ||
        object_table_size > payload.size - header.object_table_offset ||
        header.component_table_offset > payload.size ||
        component_table_size > payload.size - header.component_table_offset ||
        header.string_table_offset > payload.size ||
        header.string_table_size > payload.size - header.string_table_offset)
    {
        return AssetError("casset payload tables are outside the payload");
    }

    object_table = {payload.data + header.object_table_offset, object_table_size};
    component_table = {payload.data + header.component_table_offset, component_table_size};
    string_table = {payload.data + header.string_table_offset, header.string_table_size};

    std::string_view view;
    if (!StringViewAt(header.source_path_offset, header.source_path_size, view) ||
        !StringViewAt(header.collection_name_offset, header.collection_name_size, view))
    {
        return AssetError(
            "casset source or collection name is outside the string table");
    }

    for (std::uint32_t index = 0; index < header.object_count; ++index)
    {
        CAssetObject object;
        if (!Object(index, object))
        {
            return AssetError("casset object table is invalid");
        }
        if (object.first_component > header.component_count ||
            object.component_count > header.component_count - object.first_component)
        {
            return AssetError(
                "casset object component range is invalid");
        }
    }

    for (std::uint32_t index = 0; index < header.component_count; ++index)
    {
        CAssetComponent component;
        if (!Component(index, component))
        {
            return AssetError("casset component table is invalid");
        }
    }

    return true;
}

std::string_view CAsset::SourcePath() const
{
    std::string_view view;
    StringViewAt(header.source_path_offset, header.source_path_size, view);
    return view;
}

std::string_view CAsset::CollectionName() const
{
    std::string_view view;
    StringViewAt(header.collection_name_offset, header.collection_name_size, view);
    return view;
}

bool CAsset::Object(std::uint32_t index, CAssetObject& object) const
{
    if (index >= header.object_count)
    {
        return false;
    }

    DiskCAssetObject disk_object;
    if (!ReadObjectRow(
            object_table,
            static_cast<std::size_t>(index) * sizeof(DiskCAssetObject),
            disk_object))
        return false;

    std::string_view name;
    std::string_view parent_name;
    if (!StringViewAt(disk_object.name_offset, disk_object.name_size, name) ||
        !StringViewAt(disk_object.parent_name_offset, disk_object.parent_name_size, parent_name))
    {
        return false;
    }

    object.name = name;
    object.role = disk_object.role;
    object.object_type = disk_object.object_type;
    object.parent_name = parent_name;
    object.first_component = disk_object.first_component;
    object.component_count = disk_object.component_count;
    for (std::size_t component = 0;
         component < object.world_from_local.size(); ++component)
        object.world_from_local[component] =
            disk_object.world_from_local[component];
    return true;
}

bool CAsset::Component(std::uint32_t index, CAssetComponent& component) const
{
    if (index >= header.component_count)
    {
        return false;
    }

    DiskCAssetComponent disk_component;
    if (!ReadComponentRow(
            component_table,
            static_cast<std::size_t>(index) *
                sizeof(DiskCAssetComponent),
            disk_component))
        return false;

    std::string_view path;
    if (!StringViewAt(disk_component.path_offset, disk_component.path_size, path))
    {
        return false;
    }

    component.kind = static_cast<CAssetComponentKind>(disk_component.kind);
    component.path = path;
    return true;
}

bool CAsset::Component(const CAssetObject& object, std::uint32_t local_index, CAssetComponent& component) const
{
    if (local_index >= object.component_count)
    {
        return false;
    }
    return Component(object.first_component + local_index, component);
}

bool CAsset::StringViewAt(std::uint32_t offset, std::uint32_t size, std::string_view& view) const
{
    if (offset > string_table.size || size > string_table.size - offset)
    {
        return false;
    }
    view = std::string_view(reinterpret_cast<const char*>(string_table.data + offset), size);
    return true;
}

} // namespace CEngine::Assets
