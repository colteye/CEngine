//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/scene/scene.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "scene/scene.h"

#include "animation/animation_system.h"
#ifdef CENGINE_ENABLE_AUDIO
#include "audio/audio_system.h"
#endif
#include "context.h"
#include "physics/physics_system.h"
#include "renderer/render_system.h"
#include <iostream>

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace CEngine::Scene
{
namespace
{
constexpr std::size_t MaxPendingInputs = 4096;
constexpr std::size_t ContactBatchSize = 256;

EntityContactPhase ContactPhase(PhysicsContactType type)
{
    switch (type)
    {
    case PhysicsContactType::Begin:
        return EntityContactPhase::Begin;
    case PhysicsContactType::Persist:
        return EntityContactPhase::Persist;
    case PhysicsContactType::End:
        return EntityContactPhase::End;
    }
    return EntityContactPhase::End;
}
} // namespace

/**
 * @brief TODO: Describe Scene::~Scene.
 */
Scene::~Scene()
{
    Stop();
}

/**
 * @brief TODO: Describe Scene::Activate.
 *
 * @param context TODO: Describe this parameter.
 */
void Scene::Activate(Context &context)
{
    if (active_)
    {
        return;
    }
    context.scene = this;
    active_context_ = context;
    dropped_inputs_ = 0;
    next_input_sequence_ = 1;
    for (EntityConnection &connection : connections_)
    {
        connection.times_fired = 0;
    }
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
        if (context.animations != nullptr)
        {
            context.animations->Evaluate(0.0f);
        }
        for (const auto &entity : entities_)
        {
            if (entity != nullptr && entity->Enabled())
            {
                entity->LateUpdate(context, 0.0f);
            }
        }
        ProcessPendingInputs(context);
    }
    catch (...)
    {
        active_ = true;
        Stop();
        throw;
    }
}

/**
 * @brief TODO: Describe Scene::Update.
 *
 * @param context TODO: Describe this parameter.
 * @param delta_seconds TODO: Describe this parameter.
 */
void Scene::Update(Context &context, float delta_seconds)
{
    if (!active_)
    {
        return;
    }
    if (context.rendering != nullptr)
    {
        // Advance particles from the previous tick before entities publish
        // current-tick transforms and bursts.
        context.rendering->UpdateParticles(delta_seconds);
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
        ProcessPhysicsContacts(context);
    }
    if (context.animations != nullptr)
    {
        context.animations->Evaluate(delta_seconds);
    }
    for (const auto &entity : entities_)
    {
        if (entity != nullptr && entity->Enabled())
        {
            entity->LateUpdate(context, delta_seconds);
        }
    }
    SynchronizeAudioListener(context, delta_seconds);
    scene_time_ += std::max(delta_seconds, 0.0f);
    ProcessPendingInputs(context);
}

/**
 * @brief TODO: Describe Scene::Stop.
 */
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
            std::cerr << "Entity shutdown failed while stopping the scene.\n";
        }
    }
    started_entities_.clear();
    pending_inputs_.clear();
    body_owners_.clear();
    character_owners_.clear();
    scene_time_ = 0.0;
#ifdef CENGINE_ENABLE_AUDIO
    listener_position_valid_ = false;
#endif
    active_ = false;
    active_context_ = {};
}

/**
 * @brief TODO: Describe Scene::Reserve.
 *
 * @param entity_count TODO: Describe this parameter.
 */
void Scene::Reserve(std::size_t entity_count)
{
    entities_.reserve(entity_count);
    generations_.reserve(entity_count);
}

/**
 * @brief TODO: Describe Scene::AddConnection.
 *
 * @param connection TODO: Describe this parameter.
 */
void Scene::AddConnection(EntityConnection connection)
{
    if (!IsAlive(connection.source) || !IsAlive(connection.target))
    {
        throw std::invalid_argument("connection references an invalid entity");
    }
    if (connection.event.empty() || connection.action.empty() || !std::isfinite(connection.delay_seconds) ||
        connection.delay_seconds < 0.0f)
    {
        throw std::invalid_argument("connection is invalid");
    }
    const Entity *source = GetEntity(connection.source);
    const Entity *target = GetEntity(connection.target);
    if (source == nullptr || target == nullptr || !source->HasOutput(connection.event) ||
        !target->AcceptsInput(connection.action))
    {
        throw std::invalid_argument("connection names are unsupported by their entity classes");
    }
    connections_.push_back(std::move(connection));
}

bool Scene::Emit(EntityHandle source, std::string_view event, EntityHandle activator, std::string_view parameter)
{
    Entity *source_entity = GetEntity(source);
    if (source_entity == nullptr || event.empty() || !source_entity->HasOutput(event))
    {
        return false;
    }
    bool emitted = false;
    for (EntityConnection &connection : connections_)
    {
        if (connection.source != source || connection.event != event ||
            (connection.times_to_fire != 0 && connection.times_fired >= connection.times_to_fire))
        {
            continue;
        }
        if (pending_inputs_.size() >= MaxPendingInputs)
        {
            ++dropped_inputs_;
            continue;
        }
        PendingInput pending;
        pending.target = connection.target;
        pending.source = source;
        pending.activator = activator;
        pending.input = connection.action;
        pending.parameter = connection.parameter.empty() ? std::string(parameter) : connection.parameter;
        pending.delivery_time = scene_time_ + connection.delay_seconds;
        pending.sequence = next_input_sequence_++;
        pending_inputs_.push_back(std::move(pending));
        ++connection.times_fired;
        emitted = true;
    }
    return emitted;
}

bool Scene::DispatchInput(EntityHandle target, std::string_view input, EntityHandle source, EntityHandle activator,
                          std::string_view parameter)
{
    Entity *entity = GetEntity(target);
    if (entity == nullptr || !entity->AcceptsInput(input))
    {
        return false;
    }
    Context empty_context;
    Context &context = active_ ? active_context_ : empty_context;
    return entity->HandleInput(context, {source, activator, input, parameter});
}

std::size_t Scene::FindByName(std::string_view name, EntityHandle *entities, std::size_t capacity) const
{
    if (entities == nullptr || capacity == 0 || name.empty())
    {
        return 0;
    }
    std::size_t count = 0;
    for (const auto &entity : entities_)
    {
        if (entity != nullptr && entity->Name() == name)
        {
            if (count == capacity)
            {
                break;
            }
            entities[count++] = entity->GetHandle();
        }
    }
    return count;
}

void Scene::RegisterPhysicsBody(PhysicsBodyHandle body, EntityHandle owner)
{
    if (!body || !IsAlive(owner))
    {
        throw std::invalid_argument("physics body owner is invalid");
    }
    if (body.Index() >= body_owners_.size())
    {
        body_owners_.resize(static_cast<std::size_t>(body.Index()) + 1u);
    }
    PhysicsOwner &slot = body_owners_[body.Index()];
    if (slot.entity && slot.generation == body.Generation())
    {
        throw std::invalid_argument("physics body already has an entity owner");
    }
    slot = {body.Generation(), owner};
}

void Scene::UnregisterPhysicsBody(PhysicsBodyHandle body)
{
    if (body && body.Index() < body_owners_.size() && body_owners_[body.Index()].generation == body.Generation())
    {
        body_owners_[body.Index()] = {};
    }
}

EntityHandle Scene::PhysicsBodyOwner(PhysicsBodyHandle body) const
{
    if (!body || body.Index() >= body_owners_.size())
    {
        return {};
    }
    const PhysicsOwner &slot = body_owners_[body.Index()];
    return slot.generation == body.Generation() && IsAlive(slot.entity) ? slot.entity : EntityHandle{};
}

void Scene::RegisterPhysicsCharacter(PhysicsCharacterHandle character, EntityHandle owner)
{
    if (!character || !IsAlive(owner))
    {
        throw std::invalid_argument("physics character owner is invalid");
    }
    if (character.Index() >= character_owners_.size())
    {
        character_owners_.resize(static_cast<std::size_t>(character.Index()) + 1u);
    }
    PhysicsOwner &slot = character_owners_[character.Index()];
    if (slot.entity && slot.generation == character.Generation())
    {
        throw std::invalid_argument("physics character already has an entity owner");
    }
    slot = {character.Generation(), owner};
}

void Scene::UnregisterPhysicsCharacter(PhysicsCharacterHandle character)
{
    if (character && character.Index() < character_owners_.size() &&
        character_owners_[character.Index()].generation == character.Generation())
    {
        character_owners_[character.Index()] = {};
    }
}

EntityHandle Scene::PhysicsCharacterOwner(PhysicsCharacterHandle character) const
{
    if (!character || character.Index() >= character_owners_.size())
    {
        return {};
    }
    const PhysicsOwner &slot = character_owners_[character.Index()];
    return slot.generation == character.Generation() && IsAlive(slot.entity) ? slot.entity : EntityHandle{};
}

bool Scene::SetActiveEntity(EntityHandle entity)
{
    const Entity *candidate = GetEntity(entity);
    if (entity && (candidate == nullptr || !candidate->Enabled() ||
                   (candidate->Classname() != "camera" && candidate->Classname() != "player")))
    {
        return false;
    }
    settings_.active_entity = entity;
    return true;
}

void Scene::ProcessPhysicsContacts(Context &context)
{
    if (context.physics == nullptr)
    {
        return;
    }
    std::array<PhysicsContactEvent, ContactBatchSize> events{};
    for (;;)
    {
        const std::size_t count = context.physics->DrainContactEvents(events.data(), events.size());
        for (std::size_t index = 0; index < count; ++index)
        {
            const PhysicsContactEvent &event = events[index];
            const EntityHandle first =
                event.first ? PhysicsBodyOwner(event.first) : PhysicsCharacterOwner(event.first_character);
            const EntityHandle second =
                event.second ? PhysicsBodyOwner(event.second) : PhysicsCharacterOwner(event.second_character);
            if (Entity *entity = GetEntity(first))
            {
                entity->OnPhysicsContact(context, {second, ContactPhase(event.type), event.position, event.normal,
                                                   event.sensor, static_cast<bool>(event.second_character)});
            }
            if (Entity *entity = GetEntity(second))
            {
                entity->OnPhysicsContact(context, {first, ContactPhase(event.type), event.position, -event.normal,
                                                   event.sensor, static_cast<bool>(event.first_character)});
            }
        }
        if (count < events.size())
        {
            break;
        }
    }
}

void Scene::ProcessPendingInputs(Context &context)
{
    const std::size_t prefix = pending_inputs_.size();
    std::stable_sort(pending_inputs_.begin(), pending_inputs_.begin() + static_cast<std::ptrdiff_t>(prefix),
                     [](const PendingInput &left, const PendingInput &right) {
                         if (left.delivery_time != right.delivery_time)
                         {
                             return left.delivery_time < right.delivery_time;
                         }
                         if (left.target.Value() != right.target.Value())
                         {
                             return left.target.Value() < right.target.Value();
                         }
                         return left.sequence < right.sequence;
                     });
    std::size_t processed = 0;
    while (processed < prefix && pending_inputs_[processed].delivery_time <= scene_time_)
    {
        const PendingInput pending = pending_inputs_[processed];
        if (Entity *target = GetEntity(pending.target))
        {
            target->HandleInput(context, {pending.source, pending.activator, pending.input, pending.parameter});
        }
        ++processed;
    }
    pending_inputs_.erase(pending_inputs_.begin(), pending_inputs_.begin() + static_cast<std::ptrdiff_t>(processed));
}

void Scene::SynchronizeAudioListener(Context &context, float delta_seconds)
{
#ifdef CENGINE_ENABLE_AUDIO
    if (context.audio == nullptr || context.rendering == nullptr)
    {
        return;
    }
    const Renderer::Camera &camera = context.rendering->ActiveCamera();
    Audio::Listener listener;
    listener.position = camera.position;
    listener.forward = camera.direction;
    glm::vec3 right = glm::cross(listener.forward, glm::vec3(0.0f, 0.0f, 1.0f));
    if (glm::length(right) <= 0.00001f)
    {
        right = glm::vec3(0.0f, 1.0f, 0.0f);
    }
    listener.up = glm::normalize(glm::cross(glm::normalize(right), glm::normalize(listener.forward)));
    if (listener_position_valid_ && delta_seconds > 0.0f)
    {
        listener.velocity = (listener.position - previous_listener_position_) / delta_seconds;
    }
    context.audio->SetListener(listener);
    previous_listener_position_ = listener.position;
    listener_position_valid_ = true;
#else
    (void)context;
    (void)delta_seconds;
#endif
}

/**
 * @brief TODO: Describe Scene::AddEntity.
 *
 * @param entity TODO: Describe this parameter.
 * @param name TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
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
    if (active_)
    {
        try
        {
            result.Initialize(active_context_);
            started_entities_.push_back(&result);
        }
        catch (...)
        {
            entities_[index].reset();
            ++generations_[index];
            free_slots_.push_back(index);
            --live_count_;
            throw;
        }
    }
    return result;
}

/**
 * @brief TODO: Describe Scene::DestroyEntity.
 *
 * @param entity TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool Scene::DestroyEntity(EntityHandle entity)
{
    if (!IsAlive(entity))
    {
        return false;
    }
    const std::uint32_t index = entity.Index();
    for (PhysicsOwner &owner : body_owners_)
    {
        if (owner.entity == entity)
        {
            owner = {};
        }
    }
    for (PhysicsOwner &owner : character_owners_)
    {
        if (owner.entity == entity)
        {
            owner = {};
        }
    }
    if (active_)
    {
        entities_[index]->Shutdown(active_context_);
        const auto started = std::find(started_entities_.begin(), started_entities_.end(), entities_[index].get());
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
    pending_inputs_.erase(std::remove_if(pending_inputs_.begin(), pending_inputs_.end(),
                                         [entity](const PendingInput &input) {
                                             return input.source == entity || input.target == entity ||
                                                    input.activator == entity;
                                         }),
                          pending_inputs_.end());
    if (settings_.active_entity == entity)
    {
        settings_.active_entity = {};
    }
    entities_[index].reset();
    ++generations_[index];
    if (generations_[index] == 0)
    {
        ++generations_[index];
    }
    free_slots_.push_back(index);
    --live_count_;
    return true;
}

/**
 * @brief TODO: Describe Scene::IsAlive.
 *
 * @param entity TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool Scene::IsAlive(EntityHandle entity) const
{
    const std::uint32_t index = entity.Index();
    return entity && index < entities_.size() && entities_[index] != nullptr &&
           generations_[index] == entity.Generation();
}

/**
 * @brief TODO: Describe Scene::GetEntity.
 *
 * @param entity TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
Entity *Scene::GetEntity(EntityHandle entity)
{
    return IsAlive(entity) ? entities_[entity.Index()].get() : nullptr;
}
/**
 * @brief TODO: Describe Scene::GetEntity.
 *
 * @param entity TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
const Entity *Scene::GetEntity(EntityHandle entity) const
{
    return IsAlive(entity) ? entities_[entity.Index()].get() : nullptr;
}

/**
 * @brief TODO: Describe Scene::SetAssetReferences.
 *
 * @param references TODO: Describe this parameter.
 */
void Scene::SetAssetReferences(std::vector<Assets::Reference> references)
{
    asset_references_ = std::move(references);
}

/**
 * @brief TODO: Describe Scene::AssetReference.
 *
 * @param index TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
const Assets::Reference *Scene::AssetReference(std::uint32_t index) const
{
    return index < asset_references_.size() ? &asset_references_[index] : nullptr;
}

/**
 * @brief TODO: Describe Scene::AppendAuxiliaryAsset.
 *
 * @param asset TODO: Describe this parameter.
 */
void Scene::AppendAuxiliaryAsset(const Assets::Reference &asset)
{
    auxiliary_assets_.push_back(asset);
}

/**
 * @brief TODO: Describe Scene::AuxiliaryAsset.
 *
 * @param index TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
const Assets::Reference *Scene::AuxiliaryAsset(std::uint32_t index) const
{
    return index < auxiliary_assets_.size() ? &auxiliary_assets_[index] : nullptr;
}

} // namespace CEngine::Scene
