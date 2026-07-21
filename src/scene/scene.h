#ifndef CENGINE_SCENE_SCENE_H
#define CENGINE_SCENE_SCENE_H

#include "assets/asset_handle.h"
#include "entity/entity.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace CEngine::Assets { class AssetDatabase; enum class AssetType : std::uint32_t; }
namespace CEngine::Scene {

struct EntityConnection {
    EntityId source;
    EntityId target;
    std::string event;
    std::string action;
    float delay_seconds = 0.0f;
};

class Scene {
public:
    ~Scene();

    void Reserve(std::size_t entity_count);
    Entity& CreateEntity(std::string_view classname, std::string_view name = {});

    bool DestroyEntity(EntityId entity);
    bool IsAlive(EntityId entity) const;
    Entity* GetEntity(EntityId entity);
    const Entity* GetEntity(EntityId entity) const;
    const std::vector<std::unique_ptr<Entity>>& Entities() const { return entities_; }
    std::size_t EntityCount() const { return live_count_; }
    const std::vector<EntityConnection>& Connections() const { return connections_; }
    void AddConnection(EntityConnection connection);
    bool Active() const { return active_; }

    void Activate();
    void Stop() noexcept;

    void AdoptAssetReferences(Assets::AssetDatabase& database,
        std::vector<Assets::AssetHandle> handles);
    Assets::AssetHandle AssetReference(std::uint32_t index) const;
    Assets::AssetType ReferencedAssetType(std::uint32_t index) const;
    std::size_t AssetReferenceCount() const { return asset_references_.size(); }
    std::uint32_t AppendMaterial(Assets::AssetHandle material);
    Assets::AssetHandle Material(std::uint32_t index) const;

private:
    std::vector<std::unique_ptr<Entity>> entities_;
    std::vector<std::uint32_t> generations_;
    std::vector<std::uint32_t> free_slots_;
    Assets::AssetDatabase* asset_database_ = nullptr;
    std::vector<Assets::AssetHandle> asset_references_;
    std::vector<Assets::AssetHandle> materials_;
    std::vector<Entity*> started_entities_;
    std::vector<EntityConnection> connections_;
    std::size_t live_count_ = 0;
    bool active_ = false;
};

} // namespace CEngine::Scene

#endif
