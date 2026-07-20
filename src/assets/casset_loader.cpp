#include "assets/casset_loader.h"

#include <cstring>

namespace CEngine::Assets {
namespace {

void SetError(std::string* error, const std::string& message)
{
    if (error != nullptr)
    {
        *error = message;
    }
}

} // namespace

bool CAsset::Load(const std::filesystem::path& path, std::string* error)
{
    file = {};
    header = {};
    object_table = {};
    component_table = {};
    string_table = {};

    if (!file.Load(path, error))
    {
        return false;
    }
    return Parse(error);
}

bool CAsset::Parse(std::string* error)
{
    if (file.Type() != AssetType::Asset)
    {
        SetError(error, "asset is not a .casset composition");
        return false;
    }

    const ByteView payload = file.Payload();
    if (payload.size < sizeof(DiskCAssetHeader))
    {
        SetError(error, "casset payload is invalid");
        return false;
    }

    std::memcpy(&header, payload.data, sizeof(header));
    if (header.magic != CAssetPayloadMagic ||
        header.version != CAssetPayloadVersion ||
        header.header_size != sizeof(DiskCAssetHeader))
    {
        SetError(error, "casset payload header is not supported");
        return false;
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
        SetError(error, "casset payload tables are outside the payload");
        return false;
    }

    object_table = {payload.data + header.object_table_offset, object_table_size};
    component_table = {payload.data + header.component_table_offset, component_table_size};
    string_table = {payload.data + header.string_table_offset, header.string_table_size};

    std::string_view view;
    if (!StringViewAt(header.source_path_offset, header.source_path_size, view) ||
        !StringViewAt(header.collection_name_offset, header.collection_name_size, view))
    {
        SetError(error, "casset source or collection name is outside the string table");
        return false;
    }

    for (std::uint32_t index = 0; index < header.object_count; ++index)
    {
        CAssetObject object;
        if (!Object(index, object))
        {
            SetError(error, "casset object table is invalid");
            return false;
        }
        if (object.first_component > header.component_count ||
            object.component_count > header.component_count - object.first_component)
        {
            SetError(error, "casset object component range is invalid");
            return false;
        }
    }

    for (std::uint32_t index = 0; index < header.component_count; ++index)
    {
        CAssetComponent component;
        if (!Component(index, component))
        {
            SetError(error, "casset component table is invalid");
            return false;
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
    std::memcpy(
        &disk_object,
        object_table.data + static_cast<std::size_t>(index) * sizeof(DiskCAssetObject),
        sizeof(disk_object));

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
    std::memcpy(object.world_from_local.data(), disk_object.world_from_local, sizeof(disk_object.world_from_local));
    return true;
}

bool CAsset::Component(std::uint32_t index, CAssetComponent& component) const
{
    if (index >= header.component_count)
    {
        return false;
    }

    DiskCAssetComponent disk_component;
    std::memcpy(
        &disk_component,
        component_table.data + static_cast<std::size_t>(index) * sizeof(DiskCAssetComponent),
        sizeof(disk_component));

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
