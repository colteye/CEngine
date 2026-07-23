#ifndef CENGINE_SKELETON_LOADER_H
#define CENGINE_SKELETON_LOADER_H

#include "assets/asset_io.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace CEngine::Assets
{

constexpr std::array<char, 4> SkeletonPayloadMagic = {'C', 'E', 'S', 'K'};
constexpr std::uint16_t SkeletonPayloadVersion = 1;

#pragma pack(push, 1)
struct DiskSkeletonHeader
{
    std::array<char, 4> magic = SkeletonPayloadMagic;
    std::uint16_t version = SkeletonPayloadVersion;
    std::uint16_t header_size = 0;
    std::uint32_t bone_count = 0;
    std::uint32_t bone_table_offset = 0;
    std::uint32_t string_table_offset = 0;
    std::uint32_t string_table_size = 0;
    std::uint32_t armature_name_offset = 0;
    std::uint32_t armature_name_size = 0;
};

struct DiskSkeletonBone
{
    std::int32_t parent_index = -1;
    std::uint32_t name_offset = 0;
    std::uint32_t name_size = 0;
    float armature_from_bone[16] = {};
};
#pragma pack(pop)

static_assert(sizeof(DiskSkeletonHeader) == 32, "DiskSkeletonHeader must stay packed and stable.");
static_assert(sizeof(DiskSkeletonBone) == 76, "DiskSkeletonBone must stay packed and stable.");

class SkeletonAsset
{
  public:
    bool Load(const std::filesystem::path &path);

    [[nodiscard]] std::uint32_t BoneCount() const
    {
        return header_.bone_count;
    }
    [[nodiscard]] std::string_view ArmatureName() const;
    bool Bone(std::uint32_t index, DiskSkeletonBone &bone) const;
    [[nodiscard]] std::string_view BoneName(std::uint32_t index) const;

  private:
    bool Parse();
    bool StringViewAt(std::uint32_t offset, std::uint32_t size, std::string_view &view) const;

    AssetFile file_;
    DiskSkeletonHeader header_;
    ByteView bone_table_;
    ByteView string_table_;
};

} // namespace CEngine::Assets

#endif
