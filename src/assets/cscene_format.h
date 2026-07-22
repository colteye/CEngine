#ifndef CENGINE_ASSETS_CSCENE_FORMAT_H
#define CENGINE_ASSETS_CSCENE_FORMAT_H

#include "assets/asset_format.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace CEngine::Assets::CSceneFormat {

constexpr std::array<char, 4> SceneMagic = {'C', 'S', 'C', 'N'};
constexpr std::uint16_t SceneContainerVersion = 4;
constexpr std::uint16_t SceneEntityClassVersion = 1;
constexpr std::uint32_t InvalidEntityIndex = 0xffffffffu;
constexpr std::uint32_t InvalidAssetIndex = 0xffffffffu;

enum EntityFlags : std::uint32_t {
    EntityEnabled = 1u << 0u,
};

enum PropFlags : std::uint32_t {
    PropVisible = 1u << 0u,
    PropCollisionEnabled = 1u << 1u,
    PropDynamic = 1u << 2u,
    PropShadowOnly = 1u << 3u,
};

enum LightFlags : std::uint32_t {
    LightEnabled = 1u << 0u,
    LightCastsShadow = 1u << 1u,
};

enum EntityClassBlockFlags : std::uint32_t {
    EntityClassBlockRequired = 1u << 0u,
};

enum class LightType : std::uint32_t { Point = 0, Sun = 1, Spot = 2, Area = 3 };
enum class LightMode : std::uint32_t { Realtime = 0, Baked = 1, Mixed = 2 };
enum class CameraProjection : std::uint32_t { Perspective = 0, Orthographic = 1 };

#pragma pack(push, 1)
struct DiskSceneHeader {
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

struct DiskSceneSettings {
    float ambient_color[3] = {};
    float exposure = 1.0f;
    float gravity[3] = {0.0f, 0.0f, -9.81f};
    std::uint32_t active_camera_entity = InvalidEntityIndex;
    std::uint32_t reserved[4] = {};
};

struct DiskAssetReference {
    Guid guid = {};
    std::uint32_t type = 0;
    std::uint32_t flags = 0;
    std::uint32_t path_offset = 0;
    std::uint32_t path_size = 0;
};

struct DiskSceneEntity {
    std::uint32_t classname_offset = 0;
    std::uint32_t classname_size = 0;
    std::uint32_t name_offset = 0;
    std::uint32_t name_size = 0;
    std::uint32_t flags = EntityEnabled;
};

struct DiskEntityClassBlock {
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

struct DiskEntityConnection {
    std::uint32_t source_entity = InvalidEntityIndex;
    std::uint32_t target_entity = InvalidEntityIndex;
    std::uint32_t event_offset = 0;
    std::uint32_t event_size = 0;
    std::uint32_t action_offset = 0;
    std::uint32_t action_size = 0;
    float delay_seconds = 0.0f;
};

struct DiskTransform {
    float position[3] = {};
    float rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float scale[3] = {1.0f, 1.0f, 1.0f};
};

struct DiskProp {
    DiskTransform transform;
    std::uint32_t mesh_asset = InvalidAssetIndex;
    std::uint32_t first_material_asset = InvalidAssetIndex;
    std::uint32_t material_count = 0;
    std::uint32_t lightmap_asset = InvalidAssetIndex;
    float lightmap_scale[2] = {1.0f, 1.0f};
    float lightmap_offset[2] = {};
    float lightmap_rgbm_range = 8.0f;
    std::uint32_t flags = PropVisible;
    float collision_half_extents[3] = {0.5f, 0.5f, 0.5f};
    float mass = 1.0f;
};

struct DiskCameraEntity {
    DiskTransform transform;
    std::uint32_t projection = 0;
    float vertical_fov_radians = 1.0471976f;
    float orthographic_size = 10.0f;
    float near_clip = 0.1f;
    float far_clip = 1000.0f;
    std::uint32_t enabled = 1;
};

struct DiskLightEntity {
    DiskTransform transform;
    std::uint32_t type = 0;
    std::uint32_t mode = 0;
    float color[3] = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float range = 10.0f;
    float inner_angle_radians = 0.0f;
    float outer_angle_radians = 0.7853982f;
    float area_size[2] = {1.0f, 1.0f};
    std::uint32_t flags = LightEnabled | LightCastsShadow;
};

struct DiskPrefabEntity {
    DiskTransform transform;
    std::uint32_t prefab_asset = InvalidAssetIndex;
    std::uint32_t first_lightmap = InvalidAssetIndex;
    std::uint32_t lightmap_count = 0;
};

struct DiskPrefabLightmap {
    std::uint32_t object_index = 0;
    std::uint32_t lightmap_asset = InvalidAssetIndex;
    float scale[2] = {1.0f, 1.0f};
    float offset[2] = {};
    float rgbm_range = 8.0f;
};

struct DiskTriggerEntity {
    DiskTransform transform;
    float half_extents[3] = {0.5f, 0.5f, 0.5f};
    std::uint32_t flags = 1;
};

struct DiskPlayerStart {
    DiskTransform transform;
    std::uint32_t team = 0;
};
#pragma pack(pop)

static_assert(sizeof(DiskSceneHeader) == 96);
static_assert(sizeof(DiskSceneSettings) == 48);
static_assert(sizeof(DiskAssetReference) == 32);
static_assert(sizeof(DiskSceneEntity) == 20);
static_assert(sizeof(DiskEntityClassBlock) == 52);
static_assert(sizeof(DiskEntityConnection) == 28);
static_assert(sizeof(DiskTransform) == 40);
static_assert(sizeof(DiskProp) == 96);
static_assert(sizeof(DiskCameraEntity) == 64);
static_assert(sizeof(DiskLightEntity) == 88);
static_assert(sizeof(DiskPrefabEntity) == 52);
static_assert(sizeof(DiskPrefabLightmap) == 28);
static_assert(sizeof(DiskTriggerEntity) == 56);
static_assert(sizeof(DiskPlayerStart) == 44);

} // namespace CEngine::Assets::CSceneFormat

#endif
