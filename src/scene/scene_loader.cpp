#include "scene/scene_loader.h"

#include "assets/asset_database.h"
#include "assets/cscene_reader.h"
#include "entity/camera_entity.h"
#include "entity/dynamic_prop.h"
#include "entity/light_entity.h"
#include "entity/player_start.h"
#include "entity/prefab_entity.h"
#include "entity/static_prop.h"
#include "entity/trigger_entity.h"
#include "scene/scene.h"

#include <cstring>
#include <vector>

namespace CEngine::Scene {
namespace {
using namespace Assets::CSceneFormat;

bool Fail(std::string* error, const char* message)
{
    if (error != nullptr) *error = message;
    return false;
}

Assets::AssetType AssetTypeFor(std::uint32_t value)
{
    const auto type = static_cast<Assets::AssetType>(value);
    switch (type)
    {
    case Assets::AssetType::Texture:
    case Assets::AssetType::Material:
    case Assets::AssetType::Mesh:
    case Assets::AssetType::Skeleton:
    case Assets::AssetType::Animation:
    case Assets::AssetType::Physics:
    case Assets::AssetType::Prefab:
    case Assets::AssetType::Audio:
    case Assets::AssetType::Vfx:
    case Assets::AssetType::Navigation:
    case Assets::AssetType::Shader:
    case Assets::AssetType::Package:
    case Assets::AssetType::Asset:
        return type;
    default: return Assets::AssetType::Unknown;
    }
}

void CopyTransform(const DiskTransform& source, Transform& target)
{
    target.position = {source.position[0], source.position[1], source.position[2]};
    target.rotation = {source.rotation[3], source.rotation[0], source.rotation[1], source.rotation[2]};
    target.scale = {source.scale[0], source.scale[1], source.scale[2]};
    target.UpdateWorldMatrix();
}

template <typename T>
bool ReadRecord(Assets::ByteView records, std::uint32_t row, std::uint32_t stride, T& value)
{
    if (stride != sizeof(T)) return false;
    std::memcpy(&value, records.data + static_cast<std::size_t>(row) * stride, sizeof(T));
    return true;
}

bool LoadMaterials(Scene& scene, std::uint32_t first, std::uint32_t count,
    Assets::ByteView auxiliary, std::uint32_t& output_first, std::uint32_t& output_count,
    std::string* error)
{
    const std::size_t available = auxiliary.size / sizeof(std::uint32_t);
    if (count == 0) { output_first = 0; output_count = 0; return true; }
    if (first == InvalidAssetIndex || first > available || count > available - first)
        return Fail(error, "entity material range is invalid");
    for (std::uint32_t row = 0; row < count; ++row)
    {
        std::uint32_t asset_index;
        std::memcpy(&asset_index, auxiliary.data + (first + row) * sizeof(asset_index), sizeof(asset_index));
        if (asset_index >= scene.AssetReferenceCount() ||
            scene.ReferencedAssetType(asset_index) != Assets::AssetType::Material)
            return Fail(error, "entity material asset is invalid");
        const std::uint32_t material = scene.AppendMaterial(scene.AssetReference(asset_index));
        if (row == 0) output_first = material;
    }
    output_count = count;
    return true;
}

bool LoadClassRecord(Scene& scene, Entity& entity, std::string_view classname,
    Assets::ByteView records, std::uint32_t row, std::uint32_t stride,
    Assets::ByteView auxiliary, std::string* error)
{
    if (classname == "empty")
    {
        DiskTransform disk;
        if (!ReadRecord(records, row, stride, disk)) return Fail(error, "empty record stride is unsupported");
        CopyTransform(disk, entity.GetTransform());
        return true;
    }
    if (classname == "prop_static")
    {
        DiskStaticProp disk;
        if (!ReadRecord(records, row, stride, disk)) return Fail(error, "static prop record stride is unsupported");
        if (disk.mesh_asset >= scene.AssetReferenceCount() ||
            scene.ReferencedAssetType(disk.mesh_asset) != Assets::AssetType::Mesh ||
            (disk.lightmap_asset != InvalidAssetIndex && disk.lightmap_asset >= scene.AssetReferenceCount()))
            return Fail(error, "static prop asset index is invalid");
        if (disk.lightmap_asset != InvalidAssetIndex &&
            scene.ReferencedAssetType(disk.lightmap_asset) != Assets::AssetType::Texture)
            return Fail(error, "static prop lightmap asset is invalid");
		if (disk.lightmap_asset != InvalidAssetIndex &&
			(disk.lightmap_scale[0] <= 0.0f || disk.lightmap_scale[1] <= 0.0f ||
			 disk.lightmap_offset[0] < 0.0f || disk.lightmap_offset[1] < 0.0f ||
			 disk.lightmap_offset[0] + disk.lightmap_scale[0] > 1.0f ||
			 disk.lightmap_offset[1] + disk.lightmap_scale[1] > 1.0f ||
			 disk.lightmap_rgbm_range <= 0.0f))
			return Fail(error, "static prop lightmap binding is invalid");
        auto& prop = static_cast<Entities::StaticProp&>(entity);
        CopyTransform(disk.transform, prop.GetTransform());
        prop.mesh = scene.AssetReference(disk.mesh_asset);
        prop.lightmap = scene.AssetReference(disk.lightmap_asset);
        prop.lightmap_scale = {disk.lightmap_scale[0], disk.lightmap_scale[1]};
        prop.lightmap_offset = {disk.lightmap_offset[0], disk.lightmap_offset[1]};
        prop.lightmap_rgbm_range = disk.lightmap_rgbm_range;
        prop.visible = disk.visible != 0;
        return LoadMaterials(scene, disk.first_material_asset, disk.material_count, auxiliary,
            prop.first_material, prop.material_count, error);
    }
    if (classname == "prop_dynamic")
    {
        DiskDynamicProp disk;
        if (!ReadRecord(records, row, stride, disk)) return Fail(error, "dynamic prop record stride is unsupported");
        if (disk.mesh_asset >= scene.AssetReferenceCount() ||
            scene.ReferencedAssetType(disk.mesh_asset) != Assets::AssetType::Mesh)
            return Fail(error, "dynamic prop mesh index is invalid");
        auto& prop = static_cast<Entities::DynamicProp&>(entity);
        CopyTransform(disk.transform, prop.GetTransform());
        prop.mesh = scene.AssetReference(disk.mesh_asset);
        prop.visible = (disk.flags & 1u) != 0;
        prop.physics_enabled = (disk.flags & 2u) != 0;
        if (prop.physics_enabled && (disk.mass <= 0.0f || disk.collision_half_extents[0] <= 0.0f ||
            disk.collision_half_extents[1] <= 0.0f || disk.collision_half_extents[2] <= 0.0f))
            return Fail(error, "dynamic prop physics record is invalid");
        prop.collision_half_extents = {disk.collision_half_extents[0],
            disk.collision_half_extents[1], disk.collision_half_extents[2]};
        prop.mass = disk.mass;
        return LoadMaterials(scene, disk.first_material_asset, disk.material_count, auxiliary,
            prop.first_material, prop.material_count, error);
    }
    if (classname == "camera")
    {
        DiskCameraEntity disk;
        if (!ReadRecord(records, row, stride, disk)) return Fail(error, "camera record stride is unsupported");
        if (disk.projection > 1 || disk.near_clip <= 0.0f || disk.far_clip <= disk.near_clip)
            return Fail(error, "camera record is invalid");
        auto& camera = static_cast<Entities::CameraEntity&>(entity);
        CopyTransform(disk.transform, camera.GetTransform());
        camera.projection = static_cast<Entities::CameraProjection>(disk.projection);
        camera.vertical_fov_radians = disk.vertical_fov_radians;
        camera.orthographic_size = disk.orthographic_size;
        camera.near_clip = disk.near_clip;
        camera.far_clip = disk.far_clip;
        camera.enabled = disk.enabled != 0;
        return true;
    }
    if (classname == "light")
    {
        DiskLightEntity disk;
        if (!ReadRecord(records, row, stride, disk)) return Fail(error, "light record stride is unsupported");
        if (disk.type > 3 || disk.mode > 2 || disk.intensity < 0.0f || disk.range < 0.0f)
            return Fail(error, "light record is invalid");
        auto& light = static_cast<Entities::LightEntity&>(entity);
        CopyTransform(disk.transform, light.GetTransform());
        light.type = static_cast<Entities::LightType>(disk.type);
        light.mode = static_cast<Entities::LightMode>(disk.mode);
        light.color = {disk.color[0], disk.color[1], disk.color[2]};
        light.intensity = disk.intensity;
        light.range = disk.range;
        light.inner_angle_radians = disk.inner_angle_radians;
        light.outer_angle_radians = disk.outer_angle_radians;
        light.area_size = {disk.area_size[0], disk.area_size[1]};
        light.enabled = disk.enabled != 0;
        return true;
    }
    if (classname == "prefab_instance")
    {
        DiskPrefabEntity disk;
        if (!ReadRecord(records, row, stride, disk)) return Fail(error, "prefab record stride is unsupported");
        if (disk.prefab_asset >= scene.AssetReferenceCount() ||
            scene.ReferencedAssetType(disk.prefab_asset) != Assets::AssetType::Asset)
            return Fail(error, "prefab asset index is invalid");
        auto& prefab = static_cast<Entities::PrefabEntity&>(entity);
        CopyTransform(disk.transform, prefab.GetTransform());
        prefab.prefab = scene.AssetReference(disk.prefab_asset);
        return true;
    }
    if (classname == "trigger")
    {
        DiskTriggerEntity disk;
        if (!ReadRecord(records, row, stride, disk)) return Fail(error, "trigger record stride is unsupported");
        auto& trigger = static_cast<Entities::TriggerEntity&>(entity);
        CopyTransform(disk.transform, trigger.GetTransform());
        trigger.half_extents = {disk.half_extents[0], disk.half_extents[1], disk.half_extents[2]};
        trigger.enabled = (disk.flags & 1u) != 0;
        trigger.trigger_once = (disk.flags & 2u) != 0;
        return true;
    }
    if (classname == "info_player_start")
    {
        DiskPlayerStart disk;
        if (!ReadRecord(records, row, stride, disk)) return Fail(error, "player start record stride is unsupported");
        auto& start = static_cast<Entities::PlayerStart&>(entity);
        CopyTransform(disk.transform, start.GetTransform());
        start.team = disk.team;
        return true;
    }
    return Fail(error, "scene contains an unsupported entity classname");
}
} // namespace

std::unique_ptr<Scene> LoadScene(const std::filesystem::path& path,
    Assets::AssetDatabase& database, std::string* error)
{
    Assets::CSceneFile file;
    if (!file.Load(path, error)) return nullptr;

    std::vector<Assets::AssetHandle> assets;
    assets.reserve(file.AssetReferences().size);
    for (const auto& reference : file.AssetReferences())
    {
        const Assets::AssetType type = AssetTypeFor(reference.type);
        if (type == Assets::AssetType::Unknown)
        { Fail(error, "scene asset reference type is unsupported"); return nullptr; }
        Assets::AssetHandle handle = database.Acquire(
            file.String(reference.path_offset, reference.path_size), type, reference.guid, error);
        if (!handle)
        {
            for (Assets::AssetHandle acquired : assets) database.Release(acquired);
            return nullptr;
        }
        assets.push_back(handle);
    }

    auto scene = std::make_unique<Scene>();
    scene->AdoptAssetReferences(database, std::move(assets));
    scene->Reserve(file.Entities().size);
    std::vector<Entity*> entities;
    entities.reserve(file.Entities().size);
    for (const auto& row : file.Entities())
    {
        try
        {
            Entity& entity = scene->CreateEntity(
                file.String(row.classname_offset, row.classname_size),
                file.String(row.name_offset, row.name_size));
            entity.SetFlags(row.flags);
            entities.push_back(&entity);
        }
        catch (const std::exception& exception)
        {
            if (error != nullptr) *error = exception.what();
            return nullptr;
        }
    }

    std::vector<bool> loaded(entities.size(), false);
    for (const auto& block : file.ClassBlocks())
    {
        if (block.version != SceneEntityClassVersion)
        { Fail(error, "entity class record version is unsupported"); return nullptr; }
        const std::string_view classname = file.String(block.classname_offset, block.classname_size);
        const auto indices = file.ClassEntities(block);
        const Assets::ByteView records = file.ClassRecords(block);
        for (std::uint32_t row = 0; row < block.count; ++row)
        {
            const std::uint32_t entity_index = indices[row];
            if (loaded[entity_index]) { Fail(error, "entity has more than one class record"); return nullptr; }
            if (!LoadClassRecord(*scene, *entities[entity_index], classname, records, row,
                    block.record_stride, file.ClassAuxiliary(block), error))
                return nullptr;
            loaded[entity_index] = true;
        }
    }
    for (bool value : loaded)
        if (!value) { Fail(error, "entity has no class record"); return nullptr; }
    try
    {
        for (const auto& connection : file.Connections())
            scene->AddConnection({entities[connection.source_entity]->Id(),
                entities[connection.target_entity]->Id(),
                std::string(file.String(connection.event_offset, connection.event_size)),
                std::string(file.String(connection.action_offset, connection.action_size)),
                connection.delay_seconds});
    }
    catch (const std::exception& exception)
    {
        if (error != nullptr) *error = exception.what();
        return nullptr;
    }
    try { scene->Activate(); }
    catch (const std::exception& exception)
    {
        if (error != nullptr) *error = exception.what();
        return nullptr;
    }
    catch (...)
    {
        Fail(error, "entity lifecycle activation failed");
        return nullptr;
    }
    return scene;
}

} // namespace CEngine::Scene
