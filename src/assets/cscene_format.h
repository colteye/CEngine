//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/cscene_format.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_ASSETS_CSCENE_FORMAT_H
#define CENGINE_ASSETS_CSCENE_FORMAT_H

#include "assets/asset_format.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace CEngine::Assets::CSceneFormat
{

constexpr std::array<char, 4> SceneMagic = {'C', 'S', 'C', 'N'};
constexpr std::uint16_t SceneContainerVersion = 4;
constexpr std::uint16_t SceneEntityClassVersion = 1;
constexpr std::uint32_t InvalidEntityIndex = 0xffffffffu;
constexpr std::uint32_t InvalidAssetIndex = 0xffffffffu;

inline constexpr std::uint32_t EntityEnabled = 1u << 0u;
inline constexpr std::uint32_t EntityClassBlockRequired = 1u << 0u;

#pragma pack(push, 1)
/**
 * @brief TODO: Describe DiskSceneHeader.
 */
struct DiskSceneHeader
{
    std::array<char, 4> magic = SceneMagic;
    std::uint16_t version = SceneContainerVersion;
    std::uint16_t header_size = 0;
    std::uint64_t settings_offset = 0;
    std::uint64_t asset_table_offset = 0;
    std::uint32_t asset_count = 0;
    std::uint32_t asset_stride = 0;
    std::uint64_t entity_table_offset = 0;
    std::uint32_t entity_count = 0;
    std::uint32_t entity_stride = 0;
    std::uint64_t class_table_offset = 0;
    std::uint32_t class_count = 0;
    std::uint32_t class_stride = 0;
    std::uint64_t connection_table_offset = 0;
    std::uint32_t connection_count = 0;
    std::uint32_t connection_stride = 0;
    std::uint64_t string_table_offset = 0;
    std::uint64_t string_table_size = 0;
};

/**
 * @brief TODO: Describe DiskSceneSettings.
 */
struct DiskSceneSettings
{
    float ambient_color[3] = {};
    float exposure = 1.0f;
    float gravity[3] = {0.0f, 0.0f, -9.81f};
    std::uint32_t active_entity = InvalidEntityIndex;
    std::uint32_t reserved[4] = {};
};

/**
 * @brief TODO: Describe DiskAssetReference.
 */
struct DiskAssetReference
{
    Guid guid = {};
    std::uint32_t type = 0;
    std::uint32_t flags = 0;
    std::uint32_t path_offset = 0;
    std::uint32_t path_size = 0;
};

/**
 * @brief TODO: Describe DiskSceneEntity.
 */
struct DiskSceneEntity
{
    std::uint32_t classname_offset = 0;
    std::uint32_t classname_size = 0;
    std::uint32_t name_offset = 0;
    std::uint32_t name_size = 0;
    std::uint32_t flags = EntityEnabled;
};

/**
 * @brief TODO: Describe DiskEntityClassBlock.
 */
struct DiskEntityClassBlock
{
    std::uint32_t classname_offset = 0;
    std::uint32_t classname_size = 0;
    std::uint16_t version = SceneEntityClassVersion;
    std::uint16_t flags = 0;
    std::uint32_t count = 0;
    std::uint32_t record_stride = 0;
    std::uint64_t entity_indices_offset = 0;
    std::uint64_t records_offset = 0;
    std::uint64_t auxiliary_offset = 0;
    std::uint64_t auxiliary_size = 0;
};

/**
 * @brief TODO: Describe DiskEntityConnection.
 */
struct DiskEntityConnection
{
    std::uint32_t source_entity = InvalidEntityIndex;
    std::uint32_t target_entity = InvalidEntityIndex;
    std::uint32_t event_offset = 0;
    std::uint32_t event_size = 0;
    std::uint32_t action_offset = 0;
    std::uint32_t action_size = 0;
    float delay_seconds = 0.0f;
};

/**
 * @brief TODO: Describe DiskTransform.
 */
struct DiskTransform
{
    float position[3] = {};
    float rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float scale[3] = {1.0f, 1.0f, 1.0f};
};

#pragma pack(pop)

static_assert(sizeof(DiskSceneHeader) == 96);
static_assert(sizeof(DiskSceneSettings) == 48);
static_assert(sizeof(DiskAssetReference) == 32);
static_assert(sizeof(DiskSceneEntity) == 20);
static_assert(sizeof(DiskEntityClassBlock) == 52);
static_assert(sizeof(DiskEntityConnection) == 28);
static_assert(sizeof(DiskTransform) == 40);

} // namespace CEngine::Assets::CSceneFormat

#endif
