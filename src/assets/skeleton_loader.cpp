#include "assets/skeleton_loader.h"

#include "assets/asset_error.h"
#include "assets/binary.h"

#include <cstring>

namespace CEngine::Assets {
namespace {

bool ReadHeader(ByteView bytes, DiskSkeletonHeader& value)
{
    if (bytes.size < sizeof(value)) return false;
    std::size_t offset = 0;
    std::memcpy(value.magic.data(), bytes.data, value.magic.size());
    offset += value.magic.size();
    return ReadU16LE(bytes, offset, value.version) &&
        ReadU16LE(bytes, offset, value.header_size) &&
        ReadU32LE(bytes, offset, value.bone_count) &&
        ReadU32LE(bytes, offset, value.bone_table_offset) &&
        ReadU32LE(bytes, offset, value.string_table_offset) &&
        ReadU32LE(bytes, offset, value.string_table_size) &&
        ReadU32LE(bytes, offset, value.armature_name_offset) &&
        ReadU32LE(bytes, offset, value.armature_name_size);
}

bool ReadBoneRow(
    ByteView bytes, std::size_t offset, DiskSkeletonBone& value)
{
    if (!ReadI32LE(bytes, offset, value.parent_index) ||
        !ReadU32LE(bytes, offset, value.name_offset) ||
        !ReadU32LE(bytes, offset, value.name_size))
        return false;
    for (float& component : value.armature_from_bone)
        if (!ReadF32LE(bytes, offset, component)) return false;
    return true;
}

} // namespace

bool SkeletonAsset::Load(const std::filesystem::path& path)
{
    file = {};
    header = {};
    bone_table = {};
    string_table = {};

    if (!file.Load(path))
    {
        return false;
    }
    return Parse();
}

bool SkeletonAsset::Parse()
{
    if (file.Type() != AssetType::Skeleton)
    {
        return AssetError("asset is not a skeleton");
    }

    const ByteView payload = file.Payload();
    if (payload.size < sizeof(DiskSkeletonHeader))
    {
        return AssetError("skeleton payload is invalid");
    }

    if (!ReadHeader(payload, header))
        return AssetError("skeleton payload is invalid");
    if (header.magic != SkeletonPayloadMagic ||
        header.version != SkeletonPayloadVersion ||
        header.header_size != sizeof(DiskSkeletonHeader))
    {
        return AssetError("skeleton payload header is not supported");
    }

    const std::size_t bone_table_size =
        static_cast<std::size_t>(header.bone_count) * sizeof(DiskSkeletonBone);
    if (header.bone_table_offset > payload.size ||
        bone_table_size > payload.size - header.bone_table_offset ||
        header.string_table_offset > payload.size ||
        header.string_table_size > payload.size - header.string_table_offset)
    {
        return AssetError(
            "skeleton payload tables are outside the payload");
    }

    bone_table = {payload.data + header.bone_table_offset, bone_table_size};
    string_table = {payload.data + header.string_table_offset, header.string_table_size};

    std::string_view armature_name;
    if (!StringViewAt(header.armature_name_offset, header.armature_name_size, armature_name))
    {
        return AssetError(
            "skeleton armature name is outside the string table");
    }

    for (std::uint32_t index = 0; index < header.bone_count; ++index)
    {
        DiskSkeletonBone bone;
        if (!Bone(index, bone))
        {
            return AssetError("skeleton bone table is invalid");
        }
        std::string_view bone_name;
        if (!StringViewAt(bone.name_offset, bone.name_size, bone_name))
        {
            return AssetError(
                "skeleton bone name is outside the string table");
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
    return ReadBoneRow(
        bone_table,
        static_cast<std::size_t>(index) * sizeof(DiskSkeletonBone),
        bone);
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
