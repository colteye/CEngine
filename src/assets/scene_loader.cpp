#include "assets/scene_loader.h"

#include "assets/asset_database.h"
#include "assets/binary.h"
#include "assets/cscene_format.h"
#include "assets/cscene_reader.h"
#include "entity/entity_factory.h"
#include "entity/fog_entity.h"
#include "entity/post_process_entity.h"
#include "entity/skybox_entity.h"
#include "logging/logger.h"
#include "scene/scene.h"

#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace CEngine::Assets {
namespace {
using namespace CSceneFormat;

bool Fail(std::string_view message)
{
    Logging::Logger::Get().Error("scene", message);
    return false;
}

AssetType AssetTypeFor(std::uint32_t value)
{
    const auto type = static_cast<AssetType>(value);
    switch (type)
    {
    case AssetType::Texture:
    case AssetType::Material:
    case AssetType::Mesh:
    case AssetType::Skeleton:
    case AssetType::Animation:
    case AssetType::Physics:
    case AssetType::Prefab:
    case AssetType::Audio:
    case AssetType::Vfx:
    case AssetType::Navigation:
    case AssetType::Shader:
    case AssetType::Package:
    case AssetType::Asset:
        return type;
    default:
        return AssetType::Unknown;
    }
}

struct SerializedEntity {
    std::string_view classname;
    const std::uint8_t* bytes = nullptr;
    std::size_t size = 0;
    std::uint16_t version = 0;
    std::uint32_t auxiliary_base = 0;
};

std::unique_ptr<Scene::Scene> LoadSceneInternal(
    const std::filesystem::path& path, AssetDatabase& database,
    Entities::EntityFactory& factory)
{
    CSceneFile file;
    if (!file.Load(path)) return nullptr;

    auto scene = std::make_unique<Scene::Scene>();
    std::vector<AssetHandle> assets;
    assets.reserve(file.AssetReferences().size);
    for (const auto& reference : file.AssetReferences())
    {
        const AssetType type = AssetTypeFor(reference.type);
        if (type == AssetType::Unknown)
        {
            Fail("scene asset reference type is unsupported");
            return nullptr;
        }
        AssetHandle handle = database.Acquire(
            file.String(reference.path_offset, reference.path_size), type,
            reference.guid);
        if (!handle)
        {
            for (AssetHandle acquired : assets)
                database.Release(acquired);
            return nullptr;
        }
        assets.push_back(handle);
    }
    scene->AdoptAssetReferences(database, std::move(assets));

    std::vector<SerializedEntity> records(file.Entities().size);
    for (const auto& block : file.ClassBlocks())
    {
        const ByteView auxiliary = file.ClassAuxiliary(block);
        if (auxiliary.size % sizeof(std::uint32_t) != 0u ||
            scene->AuxiliaryAssetCount() >
                std::numeric_limits<std::uint32_t>::max())
        {
            Fail("entity class auxiliary data is invalid");
            return nullptr;
        }
        const auto auxiliary_base =
            static_cast<std::uint32_t>(scene->AuxiliaryAssetCount());
        std::size_t auxiliary_offset = 0;
        while (auxiliary_offset < auxiliary.size)
        {
            std::uint32_t asset_index = 0;
            if (!ReadU32LE(
                    auxiliary, auxiliary_offset, asset_index))
            {
                Fail("entity auxiliary asset reference is truncated");
                return nullptr;
            }
            if (asset_index >= scene->AssetReferenceCount())
            {
                Fail("entity auxiliary asset reference is invalid");
                return nullptr;
            }
            scene->AppendAuxiliaryAsset(scene->AssetReference(asset_index));
        }

        const std::string_view classname =
            file.String(block.classname_offset, block.classname_size);
        const auto indices = file.ClassEntities(block);
        const ByteView bytes = file.ClassRecords(block);
        for (std::uint32_t row = 0; row < block.count; ++row)
        {
            const std::uint32_t entity_index = indices[row];
            if (records[entity_index].bytes != nullptr)
            {
                Fail("entity has more than one class record");
                return nullptr;
            }
            records[entity_index] = {
                classname,
                bytes.data + static_cast<std::size_t>(row) * block.record_stride,
                block.record_stride,
                block.version,
                auxiliary_base,
            };
        }
    }

    scene->Reserve(file.Entities().size);
    std::vector<Scene::Entity*> entities;
    entities.reserve(file.Entities().size);
    for (std::size_t index = 0; index < file.Entities().size; ++index)
    {
        const auto& row = file.Entities()[index];
        const std::string_view classname =
            file.String(row.classname_offset, row.classname_size);
        const SerializedEntity& serialized = records[index];
        if (serialized.bytes == nullptr || serialized.classname != classname)
        {
            Fail("entity has no matching class record");
            return nullptr;
        }
        std::unique_ptr<Scene::Entity> entity = factory.Load(
            classname, serialized.version, serialized.bytes, serialized.size,
            serialized.auxiliary_base);
        if (entity == nullptr) return nullptr;
        entity->SetFlags(row.flags);
        entities.push_back(&scene->AddEntity(
            std::move(entity), file.String(row.name_offset, row.name_size)));
    }

    std::uint32_t enabled_skyboxes = 0;
    std::uint32_t enabled_fogs = 0;
    std::uint32_t enabled_post_processes = 0;
    for (const auto& entity : scene->Entities())
    {
        if (entity == nullptr || !entity->Enabled()) continue;
        if (entity->Classname() == "skybox" &&
            static_cast<const Entities::SkyboxEntity&>(*entity).enabled)
            ++enabled_skyboxes;
        if (entity->Classname() == "exponential_height_fog" &&
            static_cast<const Entities::FogEntity&>(*entity).enabled)
            ++enabled_fogs;
        if (entity->Classname() == "post_process")
            ++enabled_post_processes;
    }
    if (enabled_skyboxes > 1 || enabled_fogs > 1 ||
        enabled_post_processes > 1)
    {
        Fail("scene may contain at most one enabled skybox, fog, and post-process entity");
        return nullptr;
    }

    Scene::SceneSettings settings;
    const DiskSceneSettings& disk_settings = file.Settings();
    settings.ambient_color = {
        disk_settings.ambient_color[0], disk_settings.ambient_color[1],
        disk_settings.ambient_color[2]};
    settings.exposure = disk_settings.exposure;
    settings.gravity = {
        disk_settings.gravity[0], disk_settings.gravity[1],
        disk_settings.gravity[2]};
    if (disk_settings.active_entity != InvalidEntityIndex)
    {
        Scene::Entity* active_entity = entities[disk_settings.active_entity];
        if (!active_entity->Enabled())
        {
            Fail("scene active entity is disabled");
            return nullptr;
        }
        settings.active_entity = active_entity->Id();
    }
    scene->SetSettings(settings);

    try
    {
        for (const auto& connection : file.Connections())
            scene->AddConnection({
                entities[connection.source_entity]->Id(),
                entities[connection.target_entity]->Id(),
                std::string(file.String(
                    connection.event_offset, connection.event_size)),
                std::string(file.String(
                    connection.action_offset, connection.action_size)),
                connection.delay_seconds,
            });
    }
    catch (const std::exception& exception)
    {
        Fail(exception.what());
        return nullptr;
    }
    return scene;
}

} // namespace

std::unique_ptr<Scene::Scene> LoadScene(
    const std::filesystem::path& path, AssetDatabase& database)
{
    Entities::EntityFactory factory;
    return LoadSceneInternal(path, database, factory);
}

std::unique_ptr<Scene::Scene> LoadScene(const std::filesystem::path& path,
    AssetDatabase& database, Entities::EntityFactory& entity_factory)
{
    return LoadSceneInternal(path, database, entity_factory);
}

} // namespace CEngine::Assets
