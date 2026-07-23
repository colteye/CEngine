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

struct EntityConnection
{
    EntityHandle source;
    EntityHandle target;
    std::string event;
    std::string action;
    float delay_seconds = 0.0f;
};

struct SceneSettings
{
    glm::vec3 ambient_color = glm::vec3(0.0f);
    float exposure = 1.0f;
    glm::vec3 gravity = glm::vec3(0.0f, 0.0f, -9.81f);
    EntityHandle active_entity;
};

class Scene
{
  public:
    Scene() = default;
    ~Scene();
    Scene(const Scene &) = delete;
    Scene &operator=(const Scene &) = delete;
    Scene(Scene &&) = delete;
    Scene &operator=(Scene &&) = delete;

    void Reserve(std::size_t entity_count);
    Entity &AddEntity(std::unique_ptr<Entity> entity, std::string_view name = {});

    template <typename EntityType, typename... Args>
    EntityType &CreateEntity(std::string_view name = {}, Args &&...args)
    {
        static_assert(std::is_base_of_v<Entity, EntityType>);
        return static_cast<EntityType &>(AddEntity(std::make_unique<EntityType>(std::forward<Args>(args)...), name));
    }

    bool DestroyEntity(EntityHandle entity);
    [[nodiscard]] bool IsAlive(EntityHandle entity) const;
    Entity *GetEntity(EntityHandle entity);
    [[nodiscard]] const Entity *GetEntity(EntityHandle entity) const;
    [[nodiscard]] const std::vector<std::unique_ptr<Entity>> &Entities() const
    {
        return entities_;
    }
    [[nodiscard]] std::size_t EntityCount() const
    {
        return live_count_;
    }
    [[nodiscard]] const std::vector<EntityConnection> &Connections() const
    {
        return connections_;
    }
    void AddConnection(EntityConnection connection);
    [[nodiscard]] const SceneSettings &Settings() const
    {
        return settings_;
    }
    void SetSettings(SceneSettings settings)
    {
        settings_ = settings;
    }
    void Activate(Context &context);
    void Update(Context &context, float delta_seconds);
    void Stop() noexcept;

    void SetAssetReferences(std::vector<Assets::AssetReference> references);
    [[nodiscard]] const Assets::AssetReference *AssetReference(std::uint32_t index) const;
    [[nodiscard]] std::size_t AssetReferenceCount() const
    {
        return asset_references_.size();
    }
    void AppendAuxiliaryAsset(const Assets::AssetReference &asset);
    [[nodiscard]] const Assets::AssetReference *AuxiliaryAsset(std::uint32_t index) const;
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
