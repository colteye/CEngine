//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/skeleton_loader.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "assets/skeleton_loader.h"

#include "assets/asset_error.h"
#include "assets/binary.h"

#include <cstring>

namespace CEngine::Assets
{
namespace
{

/**
 * @brief TODO: Describe ReadHeader.
 *
 * @param bytes TODO: Describe this parameter.
 * @param value TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool ReadHeader(ByteView bytes, DiskSkeletonHeader &value)
{
    if (bytes.size < sizeof(value))
    {
        return false;
    }
    std::size_t offset = 0;
    std::memcpy(value.magic.data(), bytes.data, value.magic.size());
    offset += value.magic.size();
    return ReadU16LE(bytes, offset, value.version) && ReadU16LE(bytes, offset, value.header_size) &&
           ReadU32LE(bytes, offset, value.bone_count) && ReadU32LE(bytes, offset, value.bone_table_offset) &&
           ReadU32LE(bytes, offset, value.string_table_offset) && ReadU32LE(bytes, offset, value.string_table_size) &&
           ReadU32LE(bytes, offset, value.armature_name_offset) && ReadU32LE(bytes, offset, value.armature_name_size);
}

/**
 * @brief TODO: Describe ReadBoneRow.
 *
 * @param bytes TODO: Describe this parameter.
 * @param offset TODO: Describe this parameter.
 * @param value TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool ReadBoneRow(ByteView bytes, std::size_t offset, DiskSkeletonBone &value)
{
    if (!ReadI32LE(bytes, offset, value.parent_index) || !ReadU32LE(bytes, offset, value.name_offset) ||
        !ReadU32LE(bytes, offset, value.name_size))
    {
        return false;
    }
    for (float &component : value.armature_from_bone)
    {
        if (!ReadF32LE(bytes, offset, component))
        {
            return false;
        }
    }
    return true;
}

} // namespace

/**
 * @brief TODO: Describe SkeletonAsset::Load.
 *
 * @param path TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool SkeletonAsset::Load(const std::filesystem::path &path)
{
    file_ = {};
    header_ = {};
    bone_table_ = {};
    string_table_ = {};

    if (!file_.Load(path))
    {
        return false;
    }
    return Parse();
}

/**
 * @brief TODO: Describe SkeletonAsset::Parse.
 *
 * @return TODO: Describe the return value.
 */
bool SkeletonAsset::Parse()
{
    if (file_.Type() != AssetType::Skeleton)
    {
        return AssetError("asset is not a skeleton");
    }

    const ByteView payload = file_.Payload();
    if (payload.size < sizeof(DiskSkeletonHeader))
    {
        return AssetError("skeleton payload is invalid");
    }

    if (!ReadHeader(payload, header_))
    {
        return AssetError("skeleton payload is invalid");
    }
    if (header_.magic != SkeletonPayloadMagic || header_.version != SkeletonPayloadVersion ||
        header_.header_size != sizeof(DiskSkeletonHeader))
    {
        return AssetError("skeleton payload header is not supported");
    }

    const std::size_t bone_table_size = static_cast<std::size_t>(header_.bone_count) * sizeof(DiskSkeletonBone);
    if (header_.bone_table_offset > payload.size || bone_table_size > payload.size - header_.bone_table_offset ||
        header_.string_table_offset > payload.size ||
        header_.string_table_size > payload.size - header_.string_table_offset)
    {
        return AssetError("skeleton payload tables are outside the payload");
    }

    bone_table_ = {payload.data + header_.bone_table_offset, bone_table_size};
    string_table_ = {payload.data + header_.string_table_offset, header_.string_table_size};

    std::string_view armature_name;
    if (!StringViewAt(header_.armature_name_offset, header_.armature_name_size, armature_name))
    {
        return AssetError("skeleton armature name is outside the string table");
    }

    for (std::uint32_t index = 0; index < header_.bone_count; ++index)
    {
        DiskSkeletonBone bone;
        if (!Bone(index, bone))
        {
            return AssetError("skeleton bone table is invalid");
        }
        std::string_view bone_name;
        if (!StringViewAt(bone.name_offset, bone.name_size, bone_name))
        {
            return AssetError("skeleton bone name is outside the string table");
        }
    }

    return true;
}

/**
 * @brief TODO: Describe SkeletonAsset::ArmatureName.
 *
 * @return TODO: Describe the return value.
 */
std::string_view SkeletonAsset::ArmatureName() const
{
    std::string_view view;
    StringViewAt(header_.armature_name_offset, header_.armature_name_size, view);
    return view;
}

/**
 * @brief TODO: Describe SkeletonAsset::Bone.
 *
 * @param index TODO: Describe this parameter.
 * @param bone TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool SkeletonAsset::Bone(std::uint32_t index, DiskSkeletonBone &bone) const
{
    if (index >= header_.bone_count)
    {
        return false;
    }
    return ReadBoneRow(bone_table_, static_cast<std::size_t>(index) * sizeof(DiskSkeletonBone), bone);
}

/**
 * @brief TODO: Describe SkeletonAsset::BoneName.
 *
 * @param index TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
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

/**
 * @brief TODO: Describe SkeletonAsset::StringViewAt.
 *
 * @param offset TODO: Describe this parameter.
 * @param size TODO: Describe this parameter.
 * @param view TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool SkeletonAsset::StringViewAt(std::uint32_t offset, std::uint32_t size, std::string_view &view) const
{
    if (offset > string_table_.size || size > string_table_.size - offset)
    {
        return false;
    }
    view = std::string_view(reinterpret_cast<const char *>(string_table_.data + offset), size);
    return true;
}

} // namespace CEngine::Assets
