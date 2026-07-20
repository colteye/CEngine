#include "assets/skeleton_loader.h"

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

bool SkeletonAsset::Load(const std::filesystem::path& path, std::string* error)
{
    file = {};
    header = {};
    bone_table = {};
    string_table = {};

    if (!file.Load(path, error))
    {
        return false;
    }
    return Parse(error);
}

bool SkeletonAsset::Parse(std::string* error)
{
    if (file.Type() != AssetType::Skeleton)
    {
        SetError(error, "asset is not a skeleton");
        return false;
    }

    const ByteView payload = file.Payload();
    if (payload.size < sizeof(DiskSkeletonHeader))
    {
        SetError(error, "skeleton payload is invalid");
        return false;
    }

    std::memcpy(&header, payload.data, sizeof(header));
    if (header.magic != SkeletonPayloadMagic ||
        header.version != SkeletonPayloadVersion ||
        header.header_size != sizeof(DiskSkeletonHeader))
    {
        SetError(error, "skeleton payload header is not supported");
        return false;
    }

    const std::size_t bone_table_size =
        static_cast<std::size_t>(header.bone_count) * sizeof(DiskSkeletonBone);
    if (header.bone_table_offset > payload.size ||
        bone_table_size > payload.size - header.bone_table_offset ||
        header.string_table_offset > payload.size ||
        header.string_table_size > payload.size - header.string_table_offset)
    {
        SetError(error, "skeleton payload tables are outside the payload");
        return false;
    }

    bone_table = {payload.data + header.bone_table_offset, bone_table_size};
    string_table = {payload.data + header.string_table_offset, header.string_table_size};

    std::string_view armature_name;
    if (!StringViewAt(header.armature_name_offset, header.armature_name_size, armature_name))
    {
        SetError(error, "skeleton armature name is outside the string table");
        return false;
    }

    for (std::uint32_t index = 0; index < header.bone_count; ++index)
    {
        DiskSkeletonBone bone;
        if (!Bone(index, bone))
        {
            SetError(error, "skeleton bone table is invalid");
            return false;
        }
        std::string_view bone_name;
        if (!StringViewAt(bone.name_offset, bone.name_size, bone_name))
        {
            SetError(error, "skeleton bone name is outside the string table");
            return false;
        }
    }

    return true;
}

std::string_view SkeletonAsset::ArmatureName() const
{
    std::string_view view;
    StringViewAt(header.armature_name_offset, header.armature_name_size, view);
    return view;
}

bool SkeletonAsset::Bone(std::uint32_t index, DiskSkeletonBone& bone) const
{
    if (index >= header.bone_count)
    {
        return false;
    }
    std::memcpy(
        &bone,
        bone_table.data + static_cast<std::size_t>(index) * sizeof(DiskSkeletonBone),
        sizeof(bone));
    return true;
}

std::string_view SkeletonAsset::BoneName(std::uint32_t index) const
{
    DiskSkeletonBone bone;
    if (!Bone(index, bone))
    {
        return {};
    }

    std::string_view view;
    StringViewAt(bone.name_offset, bone.name_size, view);
    return view;
}

bool SkeletonAsset::StringViewAt(std::uint32_t offset, std::uint32_t size, std::string_view& view) const
{
    if (offset > string_table.size || size > string_table.size - offset)
    {
        return false;
    }
    view = std::string_view(reinterpret_cast<const char*>(string_table.data + offset), size);
    return true;
}

} // namespace CEngine::Assets
