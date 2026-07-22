#include "scene/scene_loader.h"

#include "assets/asset_database.h"
#include "assets/asset_io.h"
#include "assets/casset_loader.h"
#include "assets/cscene_reader.h"
#include "entity/camera_entity.h"
#include "entity/light_entity.h"
#include "entity/player_start.h"
#include "entity/prefab_entity.h"
#include "entity/prop_entity.h"
#include "entity/trigger_entity.h"
#include "scene/scene.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include <glm/gtx/matrix_decompose.hpp>

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

bool LoadPrefabLightmaps(Scene& scene, Entities::PrefabEntity& prefab,
    std::uint32_t first, std::uint32_t count, Assets::ByteView auxiliary,
    std::string* error)
{
    if (auxiliary.size % sizeof(DiskPrefabLightmap) != 0)
        return Fail(error, "prefab lightmap table is invalid");
    const std::size_t available = auxiliary.size / sizeof(DiskPrefabLightmap);
    if (count == 0)
    {
        if (first != InvalidAssetIndex)
            return Fail(error, "empty prefab lightmap range is invalid");
        return true;
    }
    if (first == InvalidAssetIndex || first > available || count > available - first)
        return Fail(error, "prefab lightmap range is invalid");
    prefab.lightmaps.reserve(count);
    std::uint32_t previous_object = InvalidEntityIndex;
    for (std::uint32_t row = 0; row < count; ++row)
    {
        DiskPrefabLightmap disk;
        std::memcpy(&disk, auxiliary.data + (first + row) * sizeof(disk), sizeof(disk));
        if (disk.lightmap_asset >= scene.AssetReferenceCount() ||
            scene.ReferencedAssetType(disk.lightmap_asset) != Assets::AssetType::Texture)
            return Fail(error, "prefab lightmap asset is invalid");
        if ((row != 0 && disk.object_index <= previous_object) ||
            disk.scale[0] <= 0.0f || disk.scale[1] <= 0.0f ||
            disk.offset[0] < 0.0f || disk.offset[1] < 0.0f ||
            disk.offset[0] + disk.scale[0] > 1.0f ||
            disk.offset[1] + disk.scale[1] > 1.0f || disk.rgbm_range <= 0.0f)
            return Fail(error, "prefab lightmap binding is invalid");
        prefab.lightmaps.push_back({disk.object_index, scene.AssetReference(disk.lightmap_asset),
            {disk.scale[0], disk.scale[1]}, {disk.offset[0], disk.offset[1]}, disk.rgbm_range});
        previous_object = disk.object_index;
    }
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
    if (classname == "prop")
    {
        DiskProp disk;
        if (!ReadRecord(records, row, stride, disk)) return Fail(error, "prop record stride is unsupported");
        if (disk.mesh_asset >= scene.AssetReferenceCount() ||
            scene.ReferencedAssetType(disk.mesh_asset) != Assets::AssetType::Mesh ||
            (disk.lightmap_asset != InvalidAssetIndex && disk.lightmap_asset >= scene.AssetReferenceCount()))
            return Fail(error, "prop asset index is invalid");
        if (disk.lightmap_asset != InvalidAssetIndex &&
            scene.ReferencedAssetType(disk.lightmap_asset) != Assets::AssetType::Texture)
            return Fail(error, "prop lightmap asset is invalid");
        const bool dynamic = (disk.flags & PropDynamic) != 0;
        if (disk.lightmap_asset != InvalidAssetIndex && dynamic)
            return Fail(error, "only a static prop may have a baked lightmap");
        if (disk.lightmap_asset != InvalidAssetIndex &&
            (disk.lightmap_scale[0] <= 0.0f || disk.lightmap_scale[1] <= 0.0f ||
             disk.lightmap_offset[0] < 0.0f || disk.lightmap_offset[1] < 0.0f ||
             disk.lightmap_offset[0] + disk.lightmap_scale[0] > 1.0f ||
             disk.lightmap_offset[1] + disk.lightmap_scale[1] > 1.0f ||
             disk.lightmap_rgbm_range <= 0.0f))
            return Fail(error, "prop lightmap binding is invalid");
        const bool collision_enabled = (disk.flags & PropCollisionEnabled) != 0;
        if (collision_enabled && (disk.collision_half_extents[0] <= 0.0f ||
            disk.collision_half_extents[1] <= 0.0f || disk.collision_half_extents[2] <= 0.0f))
            return Fail(error, "prop collision dimensions are invalid");
        if (collision_enabled && dynamic && disk.mass <= 0.0f)
            return Fail(error, "dynamic prop mass is invalid");
        auto& prop = static_cast<Entities::PropEntity&>(entity);
        CopyTransform(disk.transform, prop.GetTransform());
        prop.mesh = scene.AssetReference(disk.mesh_asset);
        prop.lightmap = scene.AssetReference(disk.lightmap_asset);
        prop.lightmap_scale = {disk.lightmap_scale[0], disk.lightmap_scale[1]};
        prop.lightmap_offset = {disk.lightmap_offset[0], disk.lightmap_offset[1]};
        prop.lightmap_rgbm_range = disk.lightmap_rgbm_range;
        prop.dynamic = dynamic;
        prop.visible = (disk.flags & PropVisible) != 0;
        prop.shadow_only = (disk.flags & PropShadowOnly) != 0;
        prop.collision_enabled = collision_enabled;
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
        light.enabled = (disk.flags & LightEnabled) != 0;
        light.casts_shadows = (disk.flags & LightCastsShadow) != 0;
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
        return LoadPrefabLightmaps(scene, prefab, disk.first_lightmap, disk.lightmap_count,
            auxiliary, error);
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

bool CopyComposedTransform(const Transform& instance,
    const std::array<float, 16>& row_major_object, Transform& target, std::string* error)
{
    glm::mat4 object(1.0f);
    for (std::size_t row = 0; row < 4; ++row)
        for (std::size_t column = 0; column < 4; ++column)
            object[column][row] = row_major_object[row * 4 + column];
    const glm::mat4 world = instance.world_matrix * object;
    glm::vec3 skew;
    glm::vec4 perspective;
    if (!glm::decompose(world, target.scale, target.rotation, target.position, skew, perspective))
        return Fail(error, "prefab object transform is invalid");
    target.rotation = glm::normalize(target.rotation);
    target.dirty = true;
    target.UpdateWorldMatrix();
    return true;
}

Assets::AssetType ComponentAssetType(Assets::CAssetComponentKind kind)
{
    switch (kind)
    {
    case Assets::CAssetComponentKind::Mesh: return Assets::AssetType::Mesh;
    case Assets::CAssetComponentKind::Material: return Assets::AssetType::Material;
    case Assets::CAssetComponentKind::Skeleton: return Assets::AssetType::Skeleton;
    case Assets::CAssetComponentKind::Animation: return Assets::AssetType::Animation;
    default: return Assets::AssetType::Unknown;
    }
}

bool AcquirePrefabComponent(Scene& scene, Assets::AssetDatabase& database,
    Assets::AssetHandle prefab, const Assets::CAssetComponent& component,
    Assets::AssetHandle& handle, std::string* error)
{
    const Assets::AssetType type = ComponentAssetType(component.kind);
    if (type == Assets::AssetType::Unknown)
        return Fail(error, "prefab component type is unsupported");
    const std::filesystem::path relative =
        (std::filesystem::path(database.Path(prefab)).parent_path() / component.path).lexically_normal();
    std::string normalized;
    if (!Assets::NormalizeProjectAssetPath(relative.generic_string(), normalized))
        return Fail(error, "prefab component path is outside the project");

    Assets::AssetFile target;
    const std::filesystem::path full = database.FullPath(prefab).parent_path() / component.path;
    if (!target.Load(full.lexically_normal(), error)) return false;
    if (target.Type() != type) return Fail(error, "prefab component asset type does not match");
    handle = database.Acquire(normalized, type, target.GetGuid(), error);
    if (!handle) return false;
    try { scene.AppendAssetReference(handle); }
    catch (...)
    {
        database.Release(handle);
        throw;
    }
    return true;
}

bool ExpandPrefab(Scene& scene, Assets::AssetDatabase& database,
    const Entities::PrefabEntity& instance, std::string* error)
{
    Assets::CAsset prefab;
    if (!prefab.Load(database.FullPath(instance.prefab), error)) return false;
    if (prefab.CompositionType() != Assets::CAssetCompositionType::Prefab)
        return Fail(error, "prefab instance does not reference a prefab composition");

    std::size_t lightmap_index = 0;
    for (std::uint32_t object_index = 0; object_index < prefab.ObjectCount(); ++object_index)
    {
        Assets::CAssetObject object;
        if (!prefab.Object(object_index, object)) return Fail(error, "prefab object is invalid");
        const Entities::PrefabLightmapBinding* lightmap = nullptr;
        if (lightmap_index < instance.lightmaps.size() &&
            instance.lightmaps[lightmap_index].object_index == object_index)
        {
            lightmap = &instance.lightmaps[lightmap_index++];
        }
        if (lightmap != nullptr && object.object_type != 1)
            return Fail(error, "prefab lightmap references a non-mesh object");
        if (object.object_type != 1) continue;

        std::vector<Assets::CAssetComponent> meshes;
        std::vector<Assets::CAssetComponent> materials;
        for (std::uint32_t component_index = 0; component_index < object.component_count; ++component_index)
        {
            Assets::CAssetComponent component;
            if (!prefab.Component(object, component_index, component))
                return Fail(error, "prefab object component is invalid");
            if (component.kind == Assets::CAssetComponentKind::Mesh) meshes.push_back(component);
            else if (component.kind == Assets::CAssetComponentKind::Material) materials.push_back(component);
        }
        if (meshes.empty() || meshes.size() != materials.size())
            return Fail(error, "prefab mesh and material component counts do not match");

        for (std::size_t part = 0; part < meshes.size(); ++part)
        {
            Assets::AssetHandle mesh;
            Assets::AssetHandle material;
            if (!AcquirePrefabComponent(scene, database, instance.prefab, meshes[part], mesh, error) ||
                !AcquirePrefabComponent(scene, database, instance.prefab, materials[part], material, error))
                return false;
            const std::string name = instance.Name() + "/" + std::string(object.name) +
                (meshes.size() == 1 ? "" : ":" + std::to_string(part));
            auto& prop = static_cast<Entities::PropEntity&>(scene.CreateEntity("prop", name));
            prop.SetFlags(instance.Flags());
            if (!CopyComposedTransform(instance.GetTransform(), object.world_from_local,
                    prop.GetTransform(), error))
                return false;
            prop.mesh = mesh;
            prop.first_material = scene.AppendMaterial(material);
            prop.material_count = 1;
            prop.shadow_only = object.role == Assets::CAssetObjectRoleOccluder;
            if (lightmap != nullptr)
            {
                prop.lightmap = lightmap->lightmap;
                prop.lightmap_scale = lightmap->scale;
                prop.lightmap_offset = lightmap->offset;
                prop.lightmap_rgbm_range = lightmap->rgbm_range;
            }
        }
    }
    return lightmap_index == instance.lightmaps.size() ||
        Fail(error, "prefab lightmap object index is outside the prefab");
}

bool ExpandPrefabs(Scene& scene, Assets::AssetDatabase& database, std::string* error)
{
    std::vector<const Entities::PrefabEntity*> instances;
    for (const auto& entity : scene.Entities())
        if (entity != nullptr && entity->Classname() == "prefab_instance")
            instances.push_back(static_cast<const Entities::PrefabEntity*>(entity.get()));
    for (const Entities::PrefabEntity* instance : instances)
        if (!ExpandPrefab(scene, database, *instance, error)) return false;
    return true;
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
    if (!ExpandPrefabs(*scene, database, error)) return nullptr;
    SceneSettings settings;
    const DiskSceneSettings& disk_settings = file.Settings();
    settings.ambient_color = {disk_settings.ambient_color[0], disk_settings.ambient_color[1],
        disk_settings.ambient_color[2]};
    settings.exposure = disk_settings.exposure;
    settings.gravity = {disk_settings.gravity[0], disk_settings.gravity[1], disk_settings.gravity[2]};
    if (disk_settings.active_camera_entity != InvalidEntityIndex)
    {
        Entity* active_camera = entities[disk_settings.active_camera_entity];
        if (active_camera->Classname() != "camera")
        { Fail(error, "scene active camera does not reference a camera entity"); return nullptr; }
        const auto& camera = static_cast<const Entities::CameraEntity&>(*active_camera);
        if (!active_camera->Enabled() || !camera.enabled)
        { Fail(error, "scene active camera is disabled"); return nullptr; }
        settings.active_camera = active_camera->Id();
    }
    scene->SetSettings(settings);
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
    return scene;
}

} // namespace CEngine::Scene
