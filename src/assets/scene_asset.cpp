//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/scene_asset.cpp
 * @brief Instantiates a scene from generated wire data.
 * @author Erik Coltey
 */

#include "assets/scene_asset.h"

#include "assets/store.h"
#include "engine/engine_entities.generated.h"
#include "entity/entity_factory.h"
#include "entity/fog_entity.h"
#include "entity/post_process_entity.h"
#include "entity/skybox_entity.h"
#include "logging/logger.h"
#include "scene/scene.h"

#include <limits>

namespace CEngine::Assets
{
namespace
{
namespace Wire = Generated::EngineEntities::Wire;

struct SerializedEntity
{
    const Wire::SceneClass *type = nullptr;
    std::size_t row = 0;
    std::uint32_t auxiliary_base = 0;
};

std::unique_ptr<Scene::Scene> LoadSceneInternal(const std::filesystem::path &path, Store &store,
                                                Entities::EntityFactory &factory)
{
    Wire::Scene decoded;
    if (!Decode(path, Type::Scene, decoded))
    {
        Logging::Logger::Get().Error("assets", "scene payload is invalid");
        return nullptr;
    }

    auto scene = std::make_unique<Scene::Scene>();
    std::vector<Reference> references;
    references.reserve(decoded.assets.size());
    for (const Wire::Reference &source : decoded.assets)
    {
        Reference reference;
        if (!store.Resolve(source.path, static_cast<Type>(source.type), source.guid, reference))
        {
            return nullptr;
        }
        references.push_back(std::move(reference));
    }
    scene->SetAssetReferences(std::move(references));

    std::vector<SerializedEntity> records(decoded.entities.size());
    for (const Wire::SceneClass &type : decoded.classes)
    {
        if (type.entities.size() > std::numeric_limits<std::size_t>::max() / type.stride ||
            type.records.size() != type.entities.size() * type.stride ||
            scene->AuxiliaryAssetCount() > std::numeric_limits<std::uint32_t>::max())
        {
            Logging::Logger::Get().Error("assets", "scene class record size is invalid");
            return nullptr;
        }
        const auto auxiliary_base = static_cast<std::uint32_t>(scene->AuxiliaryAssetCount());
        for (std::uint32_t asset : type.assets)
        {
            const Reference *reference = scene->AssetReference(asset);
            if (reference == nullptr)
            {
                Logging::Logger::Get().Error("assets", "scene class asset reference is invalid");
                return nullptr;
            }
            scene->AppendAuxiliaryAsset(*reference);
        }
        for (std::size_t row = 0; row < type.entities.size(); ++row)
        {
            const std::uint32_t entity = type.entities[row];
            if (entity >= decoded.entities.size() || records[entity].type != nullptr ||
                decoded.entities[entity].class_name != type.class_name)
            {
                Logging::Logger::Get().Error("assets", "scene class entity mapping is invalid");
                return nullptr;
            }
            records[entity] = {&type, row, auxiliary_base};
        }
    }

    scene->Reserve(decoded.entities.size());
    std::vector<Scene::Entity *> entities;
    entities.reserve(decoded.entities.size());
    for (std::size_t index = 0; index < decoded.entities.size(); ++index)
    {
        const Wire::SceneEntity &row = decoded.entities[index];
        const SerializedEntity &record = records[index];
        if (record.type == nullptr)
        {
            Logging::Logger::Get().Error("assets", "scene entity has no class record");
            return nullptr;
        }
        const std::byte *bytes = record.type->records.data() + record.row * record.type->stride;
        std::unique_ptr<Scene::Entity> entity =
            factory.Load(row.class_name, record.type->version, reinterpret_cast<const std::uint8_t *>(bytes),
                         record.type->stride, record.auxiliary_base);
        if (entity == nullptr)
        {
            return nullptr;
        }
        entity->SetFlags(row.flags);
        entities.push_back(&scene->AddEntity(std::move(entity), row.name));
    }

    std::uint32_t skyboxes = 0;
    std::uint32_t fogs = 0;
    std::uint32_t post_processes = 0;
    for (const auto &entity : scene->Entities())
    {
        if (entity == nullptr || !entity->Enabled())
        {
            continue;
        }
        skyboxes += entity->Classname() == "skybox" &&
                    dynamic_cast<const Entities::SkyboxEntity &>(*entity).enabled;
        fogs += entity->Classname() == "exponential_height_fog" &&
                dynamic_cast<const Entities::FogEntity &>(*entity).enabled;
        post_processes += entity->Classname() == "post_process";
    }
    if (skyboxes > 1u || fogs > 1u || post_processes > 1u)
    {
        Logging::Logger::Get().Error("assets", "scene has duplicate global presentation entities");
        return nullptr;
    }

    Scene::SceneSettings settings;
    settings.ambient_color = {decoded.settings.ambient_color[0], decoded.settings.ambient_color[1],
                              decoded.settings.ambient_color[2]};
    settings.exposure = decoded.settings.exposure;
    settings.gravity = {decoded.settings.gravity[0], decoded.settings.gravity[1], decoded.settings.gravity[2]};
    if (decoded.settings.active_entity != 0xffffffffu)
    {
        if (decoded.settings.active_entity >= entities.size() ||
            !entities[decoded.settings.active_entity]->Enabled())
        {
            Logging::Logger::Get().Error("assets", "scene active entity is invalid");
            return nullptr;
        }
        settings.active_entity = entities[decoded.settings.active_entity]->GetHandle();
    }
    scene->SetSettings(settings);

    try
    {
        for (const Wire::SceneConnection &connection : decoded.connections)
        {
            if (connection.source >= entities.size() || connection.target >= entities.size())
            {
                throw std::runtime_error("scene connection entity is invalid");
            }
            scene->AddConnection({
                entities[connection.source]->GetHandle(),
                entities[connection.target]->GetHandle(),
                connection.event,
                connection.action,
                connection.delay,
            });
        }
    }
    catch (const std::exception &error)
    {
        Logging::Logger::Get().Error("assets", error.what());
        return nullptr;
    }
    return scene;
}
} // namespace

std::unique_ptr<Scene::Scene> LoadScene(const std::filesystem::path &path, Store &store)
{
    Entities::EntityFactory factory;
    return LoadSceneInternal(path, store, factory);
}

std::unique_ptr<Scene::Scene> LoadScene(const std::filesystem::path &path, Store &store,
                                        Entities::EntityFactory &entity_factory)
{
    return LoadSceneInternal(path, store, entity_factory);
}

} // namespace CEngine::Assets
