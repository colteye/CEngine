//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/scene/scene.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_SCENE_SCENE_H
#define CENGINE_SCENE_SCENE_H

#include "assets/asset_store.h"
#include "context.h"
#include "entity/entity.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <glm/vec3.hpp>

namespace CEngine::Scene
{

/**
 * @brief TODO: Describe EntityConnection.
 */
struct EntityConnection
{
    EntityHandle source;
    EntityHandle target;
    std::string event;
    std::string action;
    float delay_seconds = 0.0f;
};

/**
 * @brief TODO: Describe SceneSettings.
 */
struct SceneSettings
{
    glm::vec3 ambient_color = glm::vec3(0.0f);
    float exposure = 1.0f;
    glm::vec3 gravity = glm::vec3(0.0f, 0.0f, -9.81f);
    EntityHandle active_entity;
};

/**
 * @brief TODO: Describe Scene.
 */
class Scene
{
  public:
    /**
     * @brief TODO: Describe Scene.
     */
    Scene() = default;
    /**
     * @brief TODO: Describe ~Scene.
     */
    ~Scene();
    /**
     * @brief TODO: Describe Scene.
     */
    Scene(const Scene &) = delete;
    /**
     * @brief TODO: Describe operator=.
     *
     * @return TODO: Describe the return value.
     */
    Scene &operator=(const Scene &) = delete;
    /**
     * @brief TODO: Describe Scene.
     */
    Scene(Scene &&) = delete;
    /**
     * @brief TODO: Describe operator=.
     *
     * @return TODO: Describe the return value.
     */
    Scene &operator=(Scene &&) = delete;

    /**
     * @brief TODO: Describe Reserve.
     *
     * @param entity_count TODO: Describe this parameter.
     */
    void Reserve(std::size_t entity_count);
    /**
     * @brief TODO: Describe AddEntity.
     *
     * @param entity TODO: Describe this parameter.
     * @param name TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    Entity &AddEntity(std::unique_ptr<Entity> entity, std::string_view name = {});

    /**
     * @brief TODO: Describe CreateEntity.
     *
     * @tparam EntityType TODO: Describe this template parameter.
     * @tparam Args TODO: Describe this template parameter.
     * @param name TODO: Describe this parameter.
     * @param args TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    template <typename EntityType, typename... Args>
    EntityType &CreateEntity(std::string_view name = {}, Args &&...args)
    {
        static_assert(std::is_base_of_v<Entity, EntityType>);
        return static_cast<EntityType &>(AddEntity(std::make_unique<EntityType>(std::forward<Args>(args)...), name));
    }

    /**
     * @brief TODO: Describe DestroyEntity.
     *
     * @param entity TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool DestroyEntity(EntityHandle entity);
    /**
     * @brief TODO: Describe IsAlive.
     *
     * @param entity TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] bool IsAlive(EntityHandle entity) const;
    /**
     * @brief TODO: Describe GetEntity.
     *
     * @param entity TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    Entity *GetEntity(EntityHandle entity);
    /**
     * @brief TODO: Describe GetEntity.
     *
     * @param entity TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] const Entity *GetEntity(EntityHandle entity) const;
    /**
     * @brief TODO: Describe Entities.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] const std::vector<std::unique_ptr<Entity>> &Entities() const
    {
        return entities_;
    }
    /**
     * @brief TODO: Describe EntityCount.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] std::size_t EntityCount() const
    {
        return live_count_;
    }
    /**
     * @brief TODO: Describe Connections.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] const std::vector<EntityConnection> &Connections() const
    {
        return connections_;
    }
    /**
     * @brief TODO: Describe AddConnection.
     *
     * @param connection TODO: Describe this parameter.
     */
    void AddConnection(EntityConnection connection);
    /**
     * @brief TODO: Describe Settings.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] const SceneSettings &Settings() const
    {
        return settings_;
    }
    /**
     * @brief TODO: Describe SetSettings.
     *
     * @param settings TODO: Describe this parameter.
     */
    void SetSettings(SceneSettings settings)
    {
        settings_ = settings;
    }
    /**
     * @brief TODO: Describe Activate.
     *
     * @param context TODO: Describe this parameter.
     */
    void Activate(Context &context);
    /**
     * @brief TODO: Describe Update.
     *
     * @param context TODO: Describe this parameter.
     * @param delta_seconds TODO: Describe this parameter.
     */
    void Update(Context &context, float delta_seconds);
    /**
     * @brief TODO: Describe Stop.
     */
    void Stop() noexcept;

    /**
     * @brief TODO: Describe SetAssetReferences.
     *
     * @param references TODO: Describe this parameter.
     */
    void SetAssetReferences(std::vector<Assets::AssetReference> references);
    /**
     * @brief TODO: Describe AssetReference.
     *
     * @param index TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] const Assets::AssetReference *AssetReference(std::uint32_t index) const;
    /**
     * @brief TODO: Describe AssetReferenceCount.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] std::size_t AssetReferenceCount() const
    {
        return asset_references_.size();
    }
    /**
     * @brief TODO: Describe AppendAuxiliaryAsset.
     *
     * @param asset TODO: Describe this parameter.
     */
    void AppendAuxiliaryAsset(const Assets::AssetReference &asset);
    /**
     * @brief TODO: Describe AuxiliaryAsset.
     *
     * @param index TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] const Assets::AssetReference *AuxiliaryAsset(std::uint32_t index) const;
    /**
     * @brief TODO: Describe AuxiliaryAssetCount.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] std::size_t AuxiliaryAssetCount() const
    {
        return auxiliary_assets_.size();
    }

  private:
    Context active_context_;
    std::vector<std::unique_ptr<Entity>> entities_;
    std::vector<std::uint32_t> generations_;
    std::vector<std::uint32_t> free_slots_;
    std::vector<Assets::AssetReference> asset_references_;
    std::vector<Assets::AssetReference> auxiliary_assets_;
    std::vector<Entity *> started_entities_;
    std::vector<EntityConnection> connections_;
    SceneSettings settings_;
    std::size_t live_count_ = 0;
    bool active_ = false;
};

} // namespace CEngine::Scene

#endif
