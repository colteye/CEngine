#include "scene/scene.h"

#include "assets/asset_database.h"
#include "entity/entity_factory.h"

#include <algorithm>
#include <stdexcept>

namespace CEngine::Scene {

Scene::~Scene()
{
    Stop();
    if (asset_database_ != nullptr)
        for (Assets::AssetHandle handle : asset_references_) asset_database_->Release(handle);
}

void Scene::Activate()
{
    if (active_) return;
    EntityContext context{*this};
    try
    {
        started_entities_.clear();
        started_entities_.reserve(live_count_);
        for (const auto& entity : entities_)
        {
            if (entity == nullptr) continue;
            entity->OnLoaded(context);
            started_entities_.push_back(entity.get());
        }
        active_ = true;
        for (const auto& entity : entities_)
            if (entity != nullptr) entity->OnSceneReady(context);
    }
    catch (...)
    {
        active_ = true;
        Stop();
        throw;
    }
}

void Scene::Stop() noexcept
{
    if (!active_) return;
    EntityContext context{*this};
    for (auto entity = started_entities_.rbegin(); entity != started_entities_.rend(); ++entity)
    {
        try { (*entity)->OnStop(context); }
        catch (...) {}
    }
    started_entities_.clear();
    active_ = false;
}

void Scene::Reserve(std::size_t entity_count)
{
    entities_.reserve(entity_count);
    generations_.reserve(entity_count);
}

void Scene::AddConnection(EntityConnection connection)
{
    if (!IsAlive(connection.source) || !IsAlive(connection.target))
        throw std::invalid_argument("connection references an invalid entity");
    if (connection.event.empty() || connection.action.empty() || connection.delay_seconds < 0.0f)
        throw std::invalid_argument("connection is invalid");
    connections_.push_back(std::move(connection));
}

Entity& Scene::CreateEntity(std::string_view classname, std::string_view name)
{
    std::unique_ptr<Entity> entity = Entities::MakeEntity(classname);
    if (entity == nullptr) throw std::invalid_argument("unsupported entity classname");
    std::uint32_t index;
    if (free_slots_.empty())
    {
        index = static_cast<std::uint32_t>(entities_.size());
        entities_.push_back(nullptr);
        generations_.push_back(1);
    }
    else
    {
        index = free_slots_.back();
        free_slots_.pop_back();
    }
    entity->id_ = {index, generations_[index]};
    entity->name_.assign(name);
    Entity& result = *entity;
    entities_[index] = std::move(entity);
    ++live_count_;
    return result;
}

bool Scene::DestroyEntity(EntityId entity)
{
    if (!IsAlive(entity)) return false;
    if (active_)
    {
        EntityContext context{*this};
        entities_[entity.index]->OnStop(context);
        const auto started = std::find(started_entities_.begin(), started_entities_.end(), entities_[entity.index].get());
        if (started != started_entities_.end()) started_entities_.erase(started);
    }
    connections_.erase(std::remove_if(connections_.begin(), connections_.end(),
        [entity](const EntityConnection& connection) {
            return connection.source == entity || connection.target == entity;
        }), connections_.end());
    entities_[entity.index].reset();
    ++generations_[entity.index];
    if (generations_[entity.index] == 0) ++generations_[entity.index];
    free_slots_.push_back(entity.index);
    --live_count_;
    return true;
}

bool Scene::IsAlive(EntityId entity) const
{
    return entity.index < entities_.size() && entities_[entity.index] != nullptr &&
        generations_[entity.index] == entity.generation;
}

Entity* Scene::GetEntity(EntityId entity)
{ return IsAlive(entity) ? entities_[entity.index].get() : nullptr; }
const Entity* Scene::GetEntity(EntityId entity) const
{ return IsAlive(entity) ? entities_[entity.index].get() : nullptr; }

void Scene::AdoptAssetReferences(Assets::AssetDatabase& database,
    std::vector<Assets::AssetHandle> handles)
{
    asset_database_ = &database;
    asset_references_ = std::move(handles);
}

void Scene::AppendAssetReference(Assets::AssetHandle handle)
{
    if (asset_database_ == nullptr || !handle)
        throw std::invalid_argument("scene asset reference is invalid");
    asset_references_.push_back(handle);
}

Assets::AssetHandle Scene::AssetReference(std::uint32_t index) const
{ return index < asset_references_.size() ? asset_references_[index] : Assets::AssetHandle{}; }

Assets::AssetType Scene::ReferencedAssetType(std::uint32_t index) const
{
    return asset_database_ != nullptr && index < asset_references_.size() ?
        asset_database_->Type(asset_references_[index]) : Assets::AssetType::Unknown;
}

std::uint32_t Scene::AppendMaterial(Assets::AssetHandle material)
{
    const std::uint32_t index = static_cast<std::uint32_t>(materials_.size());
    materials_.push_back(material);
    return index;
}

Assets::AssetHandle Scene::Material(std::uint32_t index) const
{ return index < materials_.size() ? materials_[index] : Assets::AssetHandle{}; }

} // namespace CEngine::Scene
