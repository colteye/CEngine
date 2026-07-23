#include "scene/scene.h"

#include "engine_context.h"
#include "physics/physics_system.h"

#include <algorithm>
#include <stdexcept>

namespace CEngine::Scene
{

Scene::~Scene()
{
    Stop();
}

void Scene::Activate(EngineContext &context)
{
    if (active_)
    {
        return;
    }
    context.scene = this;
    active_context_ = context;
    try
    {
        started_entities_.clear();
        started_entities_.reserve(live_count_);
        for (const auto &entity : entities_)
        {
            if (entity == nullptr)
            {
                continue;
            }
            entity->Initialize(context);
            started_entities_.push_back(entity.get());
        }
        active_ = true;
        for (const auto &entity : entities_)
        {
            if (entity != nullptr && entity->Enabled())
            {
                entity->Update(context, 0.0f);
            }
        }
        for (const auto &entity : entities_)
        {
            if (entity != nullptr && entity->Enabled())
            {
                entity->LateUpdate(context, 0.0f);
            }
        }
    }
    catch (...)
    {
        active_ = true;
        Stop();
        throw;
    }
}

void Scene::Update(EngineContext &context, float delta_seconds)
{
    if (!active_)
    {
        return;
    }
    for (const auto &entity : entities_)
    {
        if (entity != nullptr && entity->Enabled())
        {
            entity->Update(context, delta_seconds);
        }
    }
    if (context.physics != nullptr)
    {
        context.physics->Step(delta_seconds);
    }
    for (const auto &entity : entities_)
    {
        if (entity != nullptr && entity->Enabled())
        {
            entity->LateUpdate(context, delta_seconds);
        }
    }
}

void Scene::Stop() noexcept
{
    if (!active_)
    {
        return;
    }
    for (auto entity = started_entities_.rbegin(); entity != started_entities_.rend(); ++entity)
    {
        try
        {
            (*entity)->Shutdown(active_context_);
        }
        catch (...)
        {
        }
    }
    started_entities_.clear();
    active_ = false;
    active_context_ = {};
}

void Scene::Reserve(std::size_t entity_count)
{
    entities_.reserve(entity_count);
    generations_.reserve(entity_count);
}

void Scene::AddConnection(EntityConnection connection)
{
    if (!IsAlive(connection.source) || !IsAlive(connection.target))
    {
        throw std::invalid_argument("connection references an invalid entity");
    }
    if (connection.event.empty() || connection.action.empty() || connection.delay_seconds < 0.0f)
    {
        throw std::invalid_argument("connection is invalid");
    }
    connections_.push_back(std::move(connection));
}

Entity &Scene::AddEntity(std::unique_ptr<Entity> entity, std::string_view name)
{
    if (entity == nullptr)
    {
        throw std::invalid_argument("entity must not be null");
    }
    std::uint32_t index = 0;
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
    Entity &result = *entity;
    entities_[index] = std::move(entity);
    ++live_count_;
    return result;
}

bool Scene::DestroyEntity(EntityId entity)
{
    if (!IsAlive(entity))
    {
        return false;
    }
    if (active_)
    {
        entities_[entity.index]->Shutdown(active_context_);
        const auto started =
            std::find(started_entities_.begin(), started_entities_.end(), entities_[entity.index].get());
        if (started != started_entities_.end())
        {
            started_entities_.erase(started);
        }
    }
    connections_.erase(std::remove_if(connections_.begin(), connections_.end(),
                                      [entity](const EntityConnection &connection) {
                                          return connection.source == entity || connection.target == entity;
                                      }),
                       connections_.end());
    entities_[entity.index].reset();
    ++generations_[entity.index];
    if (generations_[entity.index] == 0)
    {
        ++generations_[entity.index];
    }
    free_slots_.push_back(entity.index);
    --live_count_;
    return true;
}

bool Scene::IsAlive(EntityId entity) const
{
    return entity.index < entities_.size() && entities_[entity.index] != nullptr &&
           generations_[entity.index] == entity.generation;
}

Entity *Scene::GetEntity(EntityId entity)
{
    return IsAlive(entity) ? entities_[entity.index].get() : nullptr;
}
const Entity *Scene::GetEntity(EntityId entity) const
{
    return IsAlive(entity) ? entities_[entity.index].get() : nullptr;
}

void Scene::SetAssetReferences(std::vector<Assets::AssetReference> references)
{
    asset_references_ = std::move(references);
}

const Assets::AssetReference *Scene::AssetReference(std::uint32_t index) const
{
    return index < asset_references_.size() ? &asset_references_[index] : nullptr;
}

void Scene::AppendAuxiliaryAsset(const Assets::AssetReference &asset)
{
    auxiliary_assets_.push_back(asset);
}

const Assets::AssetReference *Scene::AuxiliaryAsset(std::uint32_t index) const
{
    return index < auxiliary_assets_.size() ? &auxiliary_assets_[index] : nullptr;
}

} // namespace CEngine::Scene
