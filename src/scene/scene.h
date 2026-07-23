#ifndef CENGINE_SCENE_SCENE_H
#define CENGINE_SCENE_SCENE_H

#include "assets/asset_handle.h"
#include "entity/entity.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <glm/vec3.hpp>

namespace CEngine::Assets { class AssetDatabase; enum class AssetType : std::uint32_t; }
namespace CEngine { struct EngineContext; }
namespace CEngine::Scene {

struct EntityConnection {
    EntityId source;
    EntityId target;
    std::string event;
    std::string action;
    float delay_seconds = 0.0f;
};

struct SceneSettings {
    glm::vec3 ambient_color = glm::vec3(0.0f);
    float exposure = 1.0f;
    glm::vec3 gravity = glm::vec3(0.0f, 0.0f, -9.81f);
    EntityId active_entity;
};

class Scene {
public:
    Scene() = default;
    ~Scene();

    void Reserve(std::size_t entity_count);
    Entity& AddEntity(
        std::unique_ptr<Entity> entity, std::string_view name = {});

    template <typename EntityType, typename... Args>
    EntityType& CreateEntity(
        std::string_view name = {}, Args&&... args)
    {
        static_assert(std::is_base_of_v<Entity, EntityType>);
        return static_cast<EntityType&>(AddEntity(
            std::make_unique<EntityType>(std::forward<Args>(args)...), name));
    }

    bool DestroyEntity(EntityId entity);
    bool IsAlive(EntityId entity) const;
    Entity* GetEntity(EntityId entity);
    const Entity* GetEntity(EntityId entity) const;
    const std::vector<std::unique_ptr<Entity>>& Entities() const { return entities_; }
    std::size_t EntityCount() const { return live_count_; }
    const std::vector<EntityConnection>& Connections() const { return connections_; }
    void AddConnection(EntityConnection connection);
    const SceneSettings& Settings() const { return settings_; }
    void SetSettings(SceneSettings settings) { settings_ = settings; }
    bool Active() const { return active_; }

    void Activate(EngineContext& context);
    void Update(EngineContext& context, float delta_seconds);
    void Stop() noexcept;

    void AdoptAssetReferences(Assets::AssetDatabase& database,
        std::vector<Assets::AssetHandle> handles);
    void AppendAssetReference(Assets::AssetHandle handle);
    Assets::AssetHandle AssetReference(std::uint32_t index) const;
    Assets::AssetType ReferencedAssetType(std::uint32_t index) const;
    std::size_t AssetReferenceCount() const { return asset_references_.size(); }
    void AppendAuxiliaryAsset(Assets::AssetHandle asset);
    Assets::AssetHandle AuxiliaryAsset(std::uint32_t index) const;
    std::size_t AuxiliaryAssetCount() const { return auxiliary_assets_.size(); }

private:
    EngineContext* active_context_ = nullptr;
    std::vector<std::unique_ptr<Entity>> entities_;
    std::vector<std::uint32_t> generations_;
    std::vector<std::uint32_t> free_slots_;
    Assets::AssetDatabase* asset_database_ = nullptr;
    std::vector<Assets::AssetHandle> asset_references_;
    std::vector<Assets::AssetHandle> auxiliary_assets_;
    std::vector<Entity*> started_entities_;
    std::vector<EntityConnection> connections_;
    SceneSettings settings_;
    std::size_t live_count_ = 0;
    bool active_ = false;
};

} // namespace CEngine::Scene

#endif
